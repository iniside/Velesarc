// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcGovernorLogic.h"

#include "Mass/EntityManager.h"
#include "Sim/ArcBodyAssignmentFragments.h"

namespace ArcGovernorLogicPrivate
{
FMassEntityHandle TakeUnassigned(TArray<FMassEntityHandle>& Unassigned)
{
	const FMassEntityHandle Handle = Unassigned.Last();
	Unassigned.Pop();
	return Handle;
}

bool StampWorkplace(
	FMassEntityManager& EntityManager,
	const FMassEntityHandle BodyHandle,
	const FMassEntityHandle Center,
	ArcWorldSim::FStaffableBuilding& Building)
{
	FArcSettlementMembershipFragment* Membership =
		EntityManager.GetFragmentDataPtr<FArcSettlementMembershipFragment>(BodyHandle);
	FArcWorkAssignmentFragment* Work = EntityManager.GetFragmentDataPtr<FArcWorkAssignmentFragment>(BodyHandle);
	FArcGatherAssignmentFragment* Gather = EntityManager.GetFragmentDataPtr<FArcGatherAssignmentFragment>(BodyHandle);
	FArcCarrierAssignmentFragment* Carrier = EntityManager.GetFragmentDataPtr<FArcCarrierAssignmentFragment>(BodyHandle);
	if (!Membership || !Work || !Gather || !Carrier
		|| Work->Workplace.IsValid() || Gather->Workplace.IsValid() || Carrier->bAssigned)
	{
		return false;
	}

	Membership->Settlement = Center;
	if (Building.bGathering)
	{
		Gather->Workplace = Building.Entity;
	}
	else
	{
		Work->Workplace = Building.Entity;
	}
	++Building.Assigned;
	return true;
}

bool StampCarrier(
	FMassEntityManager& EntityManager,
	const FMassEntityHandle BodyHandle,
	const FMassEntityHandle Center)
{
	FArcSettlementMembershipFragment* Membership =
		EntityManager.GetFragmentDataPtr<FArcSettlementMembershipFragment>(BodyHandle);
	FArcWorkAssignmentFragment* Work = EntityManager.GetFragmentDataPtr<FArcWorkAssignmentFragment>(BodyHandle);
	FArcGatherAssignmentFragment* Gather = EntityManager.GetFragmentDataPtr<FArcGatherAssignmentFragment>(BodyHandle);
	FArcCarrierAssignmentFragment* Carrier = EntityManager.GetFragmentDataPtr<FArcCarrierAssignmentFragment>(BodyHandle);
	if (!Membership || !Work || !Gather || !Carrier
		|| Work->Workplace.IsValid() || Gather->Workplace.IsValid() || Carrier->bAssigned)
	{
		return false;
	}

	Membership->Settlement = Center;
	Carrier->bAssigned = true;
	return true;
}
}

namespace ArcWorldSim
{
void AssignWorkforce(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Center,
	TArray<FMassEntityHandle>& Unassigned,
	TArray<FStaffableBuilding>& StaffableForThisCenter,
	int32 ChainCountForThisCenter,
	int32& CarrierCount)
{
	for (FStaffableBuilding& Building : StaffableForThisCenter)
	{
		if (Building.Assigned != 0 || Building.JobSlots <= 0)
		{
			continue;
		}

		while (!Unassigned.IsEmpty() && Building.Assigned == 0)
		{
			const FMassEntityHandle BodyHandle = ArcGovernorLogicPrivate::TakeUnassigned(Unassigned);
			ArcGovernorLogicPrivate::StampWorkplace(EntityManager, BodyHandle, Center, Building);
		}
	}

	const int32 DesiredCarriers = ChainCountForThisCenter;
	while (CarrierCount < DesiredCarriers && !Unassigned.IsEmpty())
	{
		const FMassEntityHandle BodyHandle = ArcGovernorLogicPrivate::TakeUnassigned(Unassigned);
		if (ArcGovernorLogicPrivate::StampCarrier(EntityManager, BodyHandle, Center))
		{
			++CarrierCount;
		}
	}

	for (FStaffableBuilding& Building : StaffableForThisCenter)
	{
		while (Building.Assigned < Building.JobSlots && !Unassigned.IsEmpty())
		{
			const FMassEntityHandle BodyHandle = ArcGovernorLogicPrivate::TakeUnassigned(Unassigned);
			ArcGovernorLogicPrivate::StampWorkplace(EntityManager, BodyHandle, Center, Building);
		}
	}
}
}
