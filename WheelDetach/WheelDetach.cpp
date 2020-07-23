#include "plugin.h"
#include "IniReader\IniReader.h"

using namespace plugin;

float minIntensity = 300.0f;
float maxHealth = 700.0f;
float radius = 1.3f;

void HideAllAtomics(RwFrame *frame)
{
	if (!rwLinkListEmpty(&frame->objectList))
	{
		RwObjectHasFrame * atomic;

		RwLLLink * current = rwLinkListGetFirstLLLink(&frame->objectList);
		RwLLLink * end = rwLinkListGetTerminator(&frame->objectList);

		current = rwLinkListGetFirstLLLink(&frame->objectList);
		while (current != end) {
			atomic = rwLLLinkGetData(current, RwObjectHasFrame, lFrame);
			atomic->object.flags &= ~0x4; // clear

			current = rwLLLinkGetNext(current);
		}
	}
	return;
}

bool IsNotLastWheel(CAutomobile *automobile, int wheelId)
{
	for (int i = 0; i < 4; ++i)
	{
		if (i != wheelId && automobile->m_damageManager.m_anWheelsStatus[i] != 2)
		{
			return true;
		}
	}
	return false;
}

class WheelDetach
{
public:
	WheelDetach()
	{
		CIniReader ini("WheelDetach.ini");
		if (ini.data.size() > 0)
		{
			minIntensity = ini.ReadFloat("Limits", "MinIntensity", minIntensity);
			maxHealth = ini.ReadFloat("Limits", "MaxHealth", maxHealth);
			radius = ini.ReadFloat("Limits", "Radius", radius);
		}

		Events::vehicleRenderEvent += [](CVehicle *vehicle)
		{
			if (vehicle && vehicle->m_fHealth < maxHealth && vehicle->m_fDamageIntensity > 1.0f &&
				(vehicle->m_nVehicleSubClass == VEHICLE_AUTOMOBILE || vehicle->m_nVehicleSubClass == VEHICLE_QUAD) && vehicle->m_nModelIndex != MODEL_RHINO)
			{
				float minIntensityNow = minIntensity * (vehicle->m_fHealth / maxHealth) / vehicle->m_pHandlingData->m_fCollisionDamageMultiplier;
				if (vehicle->m_fDamageIntensity > minIntensityNow)
				{
					CAutomobile *automobile = reinterpret_cast<CAutomobile*>(vehicle);
					CVector collisionPos = vehicle->m_vecLastCollisionPosn;

					int wheelNode = 2;
					
					while (true)
					{
						if (automobile->m_aCarNodes[wheelNode] && DistanceBetweenPoints(automobile->m_aCarNodes[wheelNode]->ltm.pos, collisionPos) < radius) break;
						wheelNode++;
						if (wheelNode > 7) return;
					}

					int wheelId = -1;
					bool isRear = false;
					switch (wheelNode)
					{
					case eCarNodes::CAR_WHEEL_LF: //5
						wheelId = 0;
						break;
					case eCarNodes::CAR_WHEEL_LB: //7
						wheelId = 1;
						isRear = true;
						break;
					case eCarNodes::CAR_WHEEL_RF: //2
						wheelId = 2;
						break;
					case eCarNodes::CAR_WHEEL_RB: //4
						wheelId = 3;
						isRear = true;
						break;
					default:
						break;
					}

					if (vehicle->IsComponentPresent(wheelNode) && IsNotLastWheel(automobile, wheelId))
					{
						if (wheelId != -1)
						{
							if (!isRear || !vehicle->m_pHandlingData->m_nModelFlags.m_bDoubleRwheels)
							{
								automobile->m_damageManager.SetWheelStatus(wheelId, 2);
							}
						}
						automobile->SpawnFlyingComponent(wheelNode, 1);
						HideAllAtomics(automobile->m_aCarNodes[wheelNode]);
					}
				}
			}
		};
	}
} wheelDetach;
