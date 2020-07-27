#include "plugin.h"
#include "IniReader\IniReader.h"
#include "CTimer.h"
#include "CGeneral.h"

using namespace plugin;

float randomChanceToBurst = 10;
float minIntensity = 300.0f;
float maxHealth = 700.0f;
float radius = 1.3f;
float minFallSpeed = 0.5f;
float suspCompressBurst = 0.1f;
float suspCompressDetach = 0.01f;

class ExtendedData {
public:
	bool isFalling;
	unsigned int lastTimeFalling;

	ExtendedData(CVehicle *vehicle)
	{
		isFalling = false;
		lastTimeFalling = 0;
	}
};
VehicleExtendedData<ExtendedData> vehExData;

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

RwFrame * HideAllNodesRecursive(RwFrame *frame)
{
	HideAllAtomics(frame);

	if (RwFrame * nextFrame = frame->child) HideAllNodesRecursive(nextFrame);
	if (RwFrame * nextFrame = frame->next)  HideAllNodesRecursive(nextFrame);
	return frame;
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

void DetachThisWheel(CVehicle *vehicle, int wheelNode, int wheelId, bool isRear)
{
	CAutomobile *automobile = reinterpret_cast<CAutomobile*>(vehicle);
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
			randomChanceToBurst = ini.ReadInteger("Limits", "RandomChanceToBurst", randomChanceToBurst);
			minFallSpeed = ini.ReadFloat("Limits", "MinFallSpeed", minFallSpeed);
			suspCompressBurst = ini.ReadFloat("Limits", "SuspCompressBurst", suspCompressBurst);
			suspCompressDetach = ini.ReadFloat("Limits", "SuspCompressDetach", suspCompressDetach);
		}
		minFallSpeed *= -1.0f;

		Events::vehicleRenderEvent += [](CVehicle *vehicle)
		{
			if (vehicle && (vehicle->m_nVehicleSubClass == VEHICLE_AUTOMOBILE || vehicle->m_nVehicleSubClass == VEHICLE_QUAD) &&
				vehicle->m_nModelIndex != MODEL_RHINO)
			{
				CAutomobile *automobile = reinterpret_cast<CAutomobile*>(vehicle);
				ExtendedData &xdata = vehExData.Get(vehicle);
				int numContactWheels = vehicle->GetNumContactWheels();

				if (numContactWheels == 0 && vehicle->m_vecMoveSpeed.z < minFallSpeed)
				{
					xdata.lastTimeFalling = CTimer::m_snTimeInMilliseconds;
					xdata.isFalling = true;
				}
				else
				{
					if (xdata.isFalling)
					{
						for (int wheelId = 0; wheelId < 4; ++wheelId)
						{
							float suspAmount = reinterpret_cast<CAutomobile*>(vehicle)->wheelsDistancesToGround2[wheelId];
							if (suspAmount < suspCompressBurst)
							{
								if (suspAmount < suspCompressDetach)
								{
									int wheelNode;
									bool isRear = false;
									switch (wheelId)
									{
									case 0:
										wheelNode = eCarNodes::CAR_WHEEL_LF;
										break;
									case 1:
										wheelNode = eCarNodes::CAR_WHEEL_LB;
										isRear = true;
										break;
									case 2:
										wheelNode = eCarNodes::CAR_WHEEL_RF;
										break;
									case 3:
										wheelNode = eCarNodes::CAR_WHEEL_RB;
										isRear = true;
										break;
									default:
										break;
									}
									DetachThisWheel(vehicle, wheelNode, wheelId, isRear);
								}
								else
								{
									vehicle->BurstTyre(wheelId, true);
								}
							}
						}
					}
					// Reset falling if touched the ground after time limit
					if (xdata.isFalling && (CTimer::m_snTimeInMilliseconds - xdata.lastTimeFalling) > 50)
					{
						xdata.isFalling = false;
						xdata.lastTimeFalling = 0;
					}
				}

				if (vehicle->m_fHealth < maxHealth)
				{
					float minIntensityNow = minIntensity * (vehicle->m_fHealth / maxHealth) / vehicle->m_pHandlingData->m_fCollisionDamageMultiplier;
					if (vehicle->m_fDamageIntensity > minIntensityNow)
					{
						CVector collisionPos = vehicle->m_vecLastCollisionPosn;

						int wheelNode = 2;

						while (true)
						{
							if (automobile->m_aCarNodes[wheelNode] && abs(DistanceBetweenPoints(automobile->m_aCarNodes[wheelNode]->ltm.pos, collisionPos)) < radius) break;
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

						if (vehicle->IsComponentPresent(wheelNode) && automobile->m_damageManager.GetWheelStatus(wheelId) != 2 && IsNotLastWheel(automobile, wheelId))
						{
							if (wheelId != -1)
							{
								if (CGeneral::GetRandomNumberInRange(0, 100) < randomChanceToBurst)
								{
									vehicle->BurstTyre(wheelId, true);
									return;
								}
								else
								{
									if (!isRear || !vehicle->m_pHandlingData->m_nModelFlags.m_bDoubleRwheels)
									{
										automobile->m_damageManager.SetWheelStatus(wheelId, 2);
									}
								}
							}
							automobile->SpawnFlyingComponent(wheelNode, 1);
							HideAllAtomics(automobile->m_aCarNodes[wheelNode]);
							if (automobile->m_aCarNodes[wheelNode]->child)
							{
								HideAllNodesRecursive(automobile->m_aCarNodes[wheelNode]->child);
							}
						}
					}
				}
			}
		};
	}
} wheelDetach;