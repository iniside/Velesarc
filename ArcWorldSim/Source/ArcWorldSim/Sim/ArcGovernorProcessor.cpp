// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcGovernorProcessor.h"

#include "ArcMass/SmartObject/ArcMassSmartObjectFragments.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "MassCommonFragments.h"
#include "Sim/ArcBodyAssignmentFragments.h"
#include "Sim/ArcBodyUtilityProcessor.h"
#include "Sim/ArcBuildingFragments.h"
#include "Sim/ArcGovernorLogic.h"
#include "Sim/ArcProductionSlots.h"
#include "Sim/ArcSettlementBounds.h"
#include "Sim/ArcSettlementCenter.h"
#include "SmartObjectDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcGovernorProcessor)

namespace ArcGovernorProcessorPrivate
{
struct FPendingStaffable
{
	FMassEntityHandle Entity;
	FVector Location = FVector::ZeroVector;
};

struct FPendingChainBuilding
{
	FVector Location = FVector::ZeroVector;
};

struct FPendingBody
{
	FMassEntityHandle Entity;
	FVector Location = FVector::ZeroVector;
	FMassEntityHandle Settlement;
	FMassEntityHandle Work;
	FMassEntityHandle Gather;
	bool bCarrier = false;
};

FMassEntityHandle ResolveCenter(
	const FMassEntityHandle Settlement,
	const FVector& Location,
	const TSet<FMassEntityHandle>& CenterEntities,
	TConstArrayView<ArcWorldSim::FSettlementBound> CenterBounds)
{
	if (Settlement.IsValid() && CenterEntities.Contains(Settlement))
	{
		return Settlement;
	}
	return ArcWorldSim::FindSettlementAt(Location, CenterBounds);
}

void AddChainBuilding(
	const FMassEntityHandle Center,
	const TSet<FMassEntityHandle>& CenterEntities,
	TMap<FMassEntityHandle, int32>& ChainCountByCenter)
{
	if (!Center.IsValid() || !CenterEntities.Contains(Center))
	{
		return;
	}
	++ChainCountByCenter.FindChecked(Center);
}
}

UArcGovernorProcessor::UArcGovernorProcessor()
	: CenterQuery(*this)
	, BodyQuery(*this)
	, EmploymentQuery(*this)
	, ItemSourceQuery(*this)
	, StorageQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteBefore.Add(UArcBodyUtilityProcessor::StaticClass()->GetFName());
	bRequiresGameThreadExecution = true;
}

void UArcGovernorProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	CenterQuery.Initialize(EntityManager);
	CenterQuery.AddRequirement<FArcSettlementCenterFragment>(EMassFragmentAccess::ReadOnly);
	CenterQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	CenterQuery.RegisterWithProcessor(*this);

	BodyQuery.Initialize(EntityManager);
	BodyQuery.AddRequirement<FArcSettlementMembershipFragment>(EMassFragmentAccess::ReadWrite);
	BodyQuery.AddRequirement<FArcWorkAssignmentFragment>(EMassFragmentAccess::ReadWrite);
	BodyQuery.AddRequirement<FArcGatherAssignmentFragment>(EMassFragmentAccess::ReadWrite);
	BodyQuery.AddRequirement<FArcCarrierAssignmentFragment>(EMassFragmentAccess::ReadWrite);
	BodyQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	BodyQuery.RegisterWithProcessor(*this);

	EmploymentQuery.Initialize(EntityManager);
	EmploymentQuery.AddRequirement<FArcEmploymentCountFragment>(EMassFragmentAccess::ReadWrite);
	EmploymentQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EmploymentQuery.AddConstSharedRequirement<FArcStaffingConfigFragment>(EMassFragmentPresence::All);
	EmploymentQuery.AddConstSharedRequirement<FArcSmartObjectDefinitionSharedFragment>(EMassFragmentPresence::All);
	EmploymentQuery.RegisterWithProcessor(*this);

	ItemSourceQuery.Initialize(EntityManager);
	ItemSourceQuery.AddConstSharedRequirement<FArcItemSourceConfigFragment>(EMassFragmentPresence::All);
	ItemSourceQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ItemSourceQuery.RegisterWithProcessor(*this);

	StorageQuery.Initialize(EntityManager);
	StorageQuery.AddConstSharedRequirement<FArcStorageConfigFragment>(EMassFragmentPresence::All);
	StorageQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	StorageQuery.RegisterWithProcessor(*this);
}

void UArcGovernorProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TArray<ArcWorldSim::FSettlementBound> CenterBounds;
	CenterQuery.ForEachEntityChunk(Context, [&CenterBounds](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FArcSettlementCenterFragment> Centers = Ctx.GetFragmentView<FArcSettlementCenterFragment>();
		const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			ArcWorldSim::FSettlementBound Bound;
			Bound.Entity = Ctx.GetEntity(EntityIt);
			Bound.Transform = Transforms[EntityIt].GetTransform();
			Bound.Extent = Centers[EntityIt].Extent;
			CenterBounds.Add(Bound);
		}
	});

	if (CenterBounds.IsEmpty())
	{
		return;
	}

	TSet<FMassEntityHandle> CenterEntities;
	TMap<FMassEntityHandle, TArray<ArcWorldSim::FStaffableBuilding>> StaffableByCenter;
	TMap<FMassEntityHandle, TArray<FMassEntityHandle>> UnassignedByCenter;
	TMap<FMassEntityHandle, int32> ChainCountByCenter;
	TMap<FMassEntityHandle, int32> CarrierCountByCenter;
	for (const ArcWorldSim::FSettlementBound& Bound : CenterBounds)
	{
		CenterEntities.Add(Bound.Entity);
		StaffableByCenter.FindOrAdd(Bound.Entity);
		UnassignedByCenter.FindOrAdd(Bound.Entity);
		ChainCountByCenter.Add(Bound.Entity, 0);
		CarrierCountByCenter.Add(Bound.Entity, 0);
	}

	TArray<ArcGovernorProcessorPrivate::FPendingStaffable> PendingStaffable;
	EmploymentQuery.ForEachEntityChunk(Context, [&PendingStaffable](FMassExecutionContext& Ctx)
	{
		TArrayView<FArcEmploymentCountFragment> Employment =
			Ctx.GetMutableFragmentView<FArcEmploymentCountFragment>();
		const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			Employment[EntityIt].EmploymentCount = 0;
			ArcGovernorProcessorPrivate::FPendingStaffable Pending;
			Pending.Entity = Ctx.GetEntity(EntityIt);
			Pending.Location = Transforms[EntityIt].GetTransform().GetLocation();
			PendingStaffable.Add(Pending);
		}
	});

	for (const ArcGovernorProcessorPrivate::FPendingStaffable& Pending : PendingStaffable)
	{
		const FMassEntityView EntityView(EntityManager, Pending.Entity);
		const FArcGatheringConfigFragment* Gathering =
			EntityView.GetConstSharedFragmentDataPtr<FArcGatheringConfigFragment>();
		const FArcSmartObjectDefinitionSharedFragment* Def =
			EntityView.GetConstSharedFragmentDataPtr<FArcSmartObjectDefinitionSharedFragment>();

		ArcWorldSim::FStaffableBuilding Building;
		Building.Entity = Pending.Entity;
		Building.Location = Pending.Location;
		Building.bGathering = Gathering != nullptr;
		Building.JobSlots = (Def && Def->SmartObjectDefinition)
			? ArcWorldSim::CountWorkerSlots(*Def->SmartObjectDefinition)
			: 0;
		if (Building.bGathering && Building.JobSlots == 0)
		{
			Building.JobSlots = 1;
		}

		const FMassEntityHandle Center = ArcWorldSim::FindSettlementAt(Pending.Location, CenterBounds);
		if (!Center.IsValid() || !CenterEntities.Contains(Center))
		{
			continue;
		}

		StaffableByCenter.FindChecked(Center).Add(Building);
		ArcGovernorProcessorPrivate::AddChainBuilding(Center, CenterEntities, ChainCountByCenter);
	}

	TArray<ArcGovernorProcessorPrivate::FPendingChainBuilding> PendingItemSources;
	ItemSourceQuery.ForEachEntityChunk(Context, [&PendingItemSources](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			ArcGovernorProcessorPrivate::FPendingChainBuilding Pending;
			Pending.Location = Transforms[EntityIt].GetTransform().GetLocation();
			PendingItemSources.Add(Pending);
		}
	});
	for (const ArcGovernorProcessorPrivate::FPendingChainBuilding& Pending : PendingItemSources)
	{
		const FMassEntityHandle Center = ArcWorldSim::FindSettlementAt(Pending.Location, CenterBounds);
		ArcGovernorProcessorPrivate::AddChainBuilding(Center, CenterEntities, ChainCountByCenter);
	}

	TArray<ArcGovernorProcessorPrivate::FPendingChainBuilding> PendingStorages;
	StorageQuery.ForEachEntityChunk(Context, [&PendingStorages](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			ArcGovernorProcessorPrivate::FPendingChainBuilding Pending;
			Pending.Location = Transforms[EntityIt].GetTransform().GetLocation();
			PendingStorages.Add(Pending);
		}
	});
	for (const ArcGovernorProcessorPrivate::FPendingChainBuilding& Pending : PendingStorages)
	{
		const FMassEntityHandle Center = ArcWorldSim::FindSettlementAt(Pending.Location, CenterBounds);
		ArcGovernorProcessorPrivate::AddChainBuilding(Center, CenterEntities, ChainCountByCenter);
	}

	TArray<ArcGovernorProcessorPrivate::FPendingBody> PendingBodies;
	BodyQuery.ForEachEntityChunk(Context, [&PendingBodies](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FArcSettlementMembershipFragment> Memberships =
			Ctx.GetFragmentView<FArcSettlementMembershipFragment>();
		const TConstArrayView<FArcWorkAssignmentFragment> WorkAssignments =
			Ctx.GetFragmentView<FArcWorkAssignmentFragment>();
		const TConstArrayView<FArcGatherAssignmentFragment> GatherAssignments =
			Ctx.GetFragmentView<FArcGatherAssignmentFragment>();
		const TConstArrayView<FArcCarrierAssignmentFragment> CarrierAssignments =
			Ctx.GetFragmentView<FArcCarrierAssignmentFragment>();
		const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			ArcGovernorProcessorPrivate::FPendingBody Pending;
			Pending.Entity = Ctx.GetEntity(EntityIt);
			Pending.Location = Transforms[EntityIt].GetTransform().GetLocation();
			Pending.Settlement = Memberships[EntityIt].Settlement;
			Pending.Work = WorkAssignments[EntityIt].Workplace;
			Pending.Gather = GatherAssignments[EntityIt].Workplace;
			Pending.bCarrier = CarrierAssignments[EntityIt].bAssigned;
			PendingBodies.Add(Pending);
		}
	});

	TMap<FMassEntityHandle, ArcWorldSim::FStaffableBuilding*> StaffableByEntity;
	for (const ArcWorldSim::FSettlementBound& Bound : CenterBounds)
	{
		TArray<ArcWorldSim::FStaffableBuilding>& Buildings = StaffableByCenter.FindChecked(Bound.Entity);
		for (ArcWorldSim::FStaffableBuilding& Building : Buildings)
		{
			StaffableByEntity.Add(Building.Entity, &Building);
		}
	}

	for (const ArcGovernorProcessorPrivate::FPendingBody& Pending : PendingBodies)
	{
		if (!Pending.Work.IsValid() && !Pending.Gather.IsValid() && !Pending.bCarrier)
		{
			const FMassEntityHandle Center = ArcGovernorProcessorPrivate::ResolveCenter(
				Pending.Settlement,
				Pending.Location,
				CenterEntities,
				CenterBounds);
			if (Center.IsValid())
			{
				UnassignedByCenter.FindChecked(Center).Add(Pending.Entity);
			}
			continue;
		}

		const FMassEntityHandle Workplace = Pending.Work.IsValid() ? Pending.Work : Pending.Gather;
		if (Workplace.IsValid())
		{
			ArcWorldSim::FStaffableBuilding** Found = StaffableByEntity.Find(Workplace);
			if (Found)
			{
				++(*Found)->Assigned;
			}
			continue;
		}

		if (Pending.bCarrier)
		{
			const FMassEntityHandle Center = ArcGovernorProcessorPrivate::ResolveCenter(
				Pending.Settlement,
				Pending.Location,
				CenterEntities,
				CenterBounds);
			if (Center.IsValid())
			{
				++CarrierCountByCenter.FindChecked(Center);
			}
		}
	}

	for (const ArcWorldSim::FSettlementBound& Bound : CenterBounds)
	{
		ArcWorldSim::AssignWorkforce(
			EntityManager,
			Bound.Entity,
			UnassignedByCenter.FindChecked(Bound.Entity),
			StaffableByCenter.FindChecked(Bound.Entity),
			ChainCountByCenter.FindChecked(Bound.Entity),
			CarrierCountByCenter.FindChecked(Bound.Entity));
	}

	for (const TPair<FMassEntityHandle, ArcWorldSim::FStaffableBuilding*>& Pair : StaffableByEntity)
	{
		FArcEmploymentCountFragment* Employment =
			EntityManager.GetFragmentDataPtr<FArcEmploymentCountFragment>(Pair.Key);
		if (Employment)
		{
			Employment->EmploymentCount = Pair.Value->Assigned;
		}
	}
}
