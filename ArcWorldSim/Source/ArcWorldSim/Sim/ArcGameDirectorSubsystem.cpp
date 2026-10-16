// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcGameDirectorSubsystem.h"

#include "ArcMass/Spawner/ArcMassSpawnerSubsystem.h"
#include "Engine/World.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Sim/ArcBodyAssignmentFragments.h"
#include "Sim/ArcBodyFragments.h"
#include "Sim/ArcSettlementCenter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcGameDirectorSubsystem)

int32 UArcGameDirectorSubsystem::SpawnPopulation(
	FMassEntityHandle Settlement,
	UArcMassEntityConfigAsset* Config,
	int32 Count)
{
	if (Count <= 0 || Config == nullptr || !Settlement.IsValid())
	{
		return 0;
	}

	UWorld* World = GetWorld();
	UMassEntitySubsystem* Mass = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	UArcMassSpawnerSubsystem* Spawner = World ? World->GetSubsystem<UArcMassSpawnerSubsystem>() : nullptr;
	if (World == nullptr || Mass == nullptr || Spawner == nullptr)
	{
		return 0;
	}

	FMassEntityManager& EntityManager = Mass->GetMutableEntityManager();
	if (!EntityManager.IsEntityActive(Settlement)
		|| EntityManager.GetFragmentDataPtr<FArcSettlementCenterFragment>(Settlement) == nullptr)
	{
		return 0;
	}

	const FTransformFragment* TransformFragment = EntityManager.GetFragmentDataPtr<FTransformFragment>(Settlement);
	if (TransformFragment == nullptr)
	{
		return 0;
	}

	const FTransform At = TransformFragment->GetTransform();

	TArray<FMassEntityHandle> Handles;
	Spawner->SpawnEntities(Config, Count, At, Handles);

	TArray<FMassEntityHandle> MissingBody;
	int32 Stamped = 0;
	for (const FMassEntityHandle& Handle : Handles)
	{
		FArcBodyFragment* Body = EntityManager.GetFragmentDataPtr<FArcBodyFragment>(Handle);
		FArcSettlementMembershipFragment* Membership =
			EntityManager.GetFragmentDataPtr<FArcSettlementMembershipFragment>(Handle);
		FArcWorkAssignmentFragment* Work = EntityManager.GetFragmentDataPtr<FArcWorkAssignmentFragment>(Handle);
		FArcGatherAssignmentFragment* Gather = EntityManager.GetFragmentDataPtr<FArcGatherAssignmentFragment>(Handle);
		FArcCarrierAssignmentFragment* Carrier = EntityManager.GetFragmentDataPtr<FArcCarrierAssignmentFragment>(Handle);
		if (Body == nullptr || Membership == nullptr || Work == nullptr || Gather == nullptr || Carrier == nullptr)
		{
			MissingBody.Add(Handle);
			continue;
		}

		Body->State = EArcBodyState::Idle;
		Body->Command.Reset();
		Body->LastCommandSerial = 0;
		Body->LastCommandResult = EArcBodyCommandResult::None;
		Membership->Settlement = Settlement;
		*Work = FArcWorkAssignmentFragment();
		*Gather = FArcGatherAssignmentFragment();
		*Carrier = FArcCarrierAssignmentFragment();
		++Stamped;
	}

	if (MissingBody.Num() > 0)
	{
		Spawner->DestroyEntities(MissingBody);
	}

	return Stamped;
}
