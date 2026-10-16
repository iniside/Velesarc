// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcStaffingProcessor.h"

#include "ArcMass/Debugger/ArcEconomyTrace.h"
#include "Mass/EntityManager.h"
#include "Mass/ExecutionContext.h"
#include "MassSmartObjectFragments.h"
#include "Sim/ArcBodyAssignmentFragments.h"
#include "Sim/ArcBodyFragments.h"
#include "Sim/ArcBuildingFragments.h"
#include "Sim/ArcEconomyNames.h"
#include "Sim/ArcSmartObjectBehaviorFragments.h"
#include "Sim/ArcSmartObjectBehaviorProcessors.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcStaffingProcessor)

namespace ArcStaffingProcessorPrivate
{
void AddStaff(FMassEntityManager& EntityManager, const FMassEntityHandle Workplace)
{
	if (!EntityManager.IsEntityValid(Workplace))
	{
		return;
	}

	FArcStaffedFragment* Staffed = EntityManager.GetFragmentDataPtr<FArcStaffedFragment>(Workplace);
	if (Staffed)
	{
		++Staffed->StaffCount;
	}
}
}

UArcStaffingProcessor::UArcStaffingProcessor()
	: BuildingQuery(*this)
	, BodyQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteAfter.Add(UArcSmartObjectWorkBehaviorProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteAfter.Add(UArcSmartObjectPickupBehaviorProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteAfter.Add(UArcSmartObjectDropBehaviorProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteAfter.Add(UArcSmartObjectGatherBehaviorProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteAfter.Add(UArcSmartObjectDrawBehaviorProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteAfter.Add(UArcSmartObjectEatBehaviorProcessor::StaticClass()->GetFName());
	bRequiresGameThreadExecution = true;
}

void UArcStaffingProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	BuildingQuery.Initialize(EntityManager);
	BuildingQuery.AddRequirement<FArcStaffedFragment>(EMassFragmentAccess::ReadWrite);
	BuildingQuery.RegisterWithProcessor(*this);

	BodyQuery.Initialize(EntityManager);
	BodyQuery.AddRequirement<FArcBodyFragment>(EMassFragmentAccess::ReadOnly);
	BodyQuery.AddRequirement<FArcWorkAssignmentFragment>(EMassFragmentAccess::ReadOnly);
	BodyQuery.AddRequirement<FMassSmartObjectUserFragment>(EMassFragmentAccess::ReadOnly);
	BodyQuery.AddSparseRequirement<FArcSmartObjectBehaviorFragment>(EMassFragmentPresence::All);
	BodyQuery.AddSparseRequirement<FArcSmartObjectWorkBehaviorFragment>(EMassFragmentPresence::All);
	BodyQuery.RegisterWithProcessor(*this);
}

void UArcStaffingProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	BuildingQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Ctx)
	{
		TArrayView<FArcStaffedFragment> StaffedViews = Ctx.GetMutableFragmentView<FArcStaffedFragment>();
		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			StaffedViews[EntityIt].StaffCount = 0;
		}
	});

	BodyQuery.ForEachEntityChunk(Context, [&EntityManager](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FArcBodyFragment> Bodies = Ctx.GetFragmentView<FArcBodyFragment>();
		const TConstArrayView<FArcWorkAssignmentFragment> WorkAssignments =
			Ctx.GetFragmentView<FArcWorkAssignmentFragment>();
		const TConstArrayView<FMassSmartObjectUserFragment> Users =
			Ctx.GetFragmentView<FMassSmartObjectUserFragment>();
		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateSparseEntityIterator(); EntityIt; ++EntityIt)
		{
			const FArcBodyFragment& Body = Bodies[EntityIt];
			const FArcSmartObjectBehaviorFragment* Lifecycle =
				Ctx.GetSparseElement<FArcSmartObjectBehaviorFragment>(EntityIt);
			const FArcSmartObjectWorkBehaviorFragment* Work =
				Ctx.GetSparseElement<FArcSmartObjectWorkBehaviorFragment>(EntityIt);
			if (Lifecycle && Work
				&& Body.State == EArcBodyState::Interaction
				&& Body.Command.IsActive()
				&& Body.Command.Serial == Lifecycle->CommandSerial
				&& Lifecycle->CommandSerial == Work->CommandSerial
				&& Users[EntityIt].InteractionStatus == EMassSmartObjectInteractionStatus::InProgress)
			{
				ArcStaffingProcessorPrivate::AddStaff(EntityManager, WorkAssignments[EntityIt].Workplace);
			}
		}
	});

	TSet<FMassEntityHandle> SeenBuildings;
	BuildingQuery.ForEachEntityChunk(Context, [this, &EntityManager, &SeenBuildings](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FArcStaffedFragment> StaffedViews = Ctx.GetFragmentView<FArcStaffedFragment>();
		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
			SeenBuildings.Add(Entity);
			const int32 NewCount = StaffedViews[EntityIt].StaffCount;
			int32& LastCount = LastPostedStaffCount.FindOrAdd(Entity);
			if (LastCount != NewCount)
			{
				TRACE_ARC_ECONOMY_BOOKMARKF(
					TEXT("STAFF-COUNT building=%s %d->%d"),
					*ArcEconomyNames::BuildingName(EntityManager, Entity),
					LastCount,
					NewCount);
				LastCount = NewCount;
			}
		}
	});

	TArray<FMassEntityHandle> StaleBuildings;
	for (const TPair<FMassEntityHandle, int32>& Pair : LastPostedStaffCount)
	{
		if (!SeenBuildings.Contains(Pair.Key))
		{
			StaleBuildings.Add(Pair.Key);
		}
	}
	for (const FMassEntityHandle& Handle : StaleBuildings)
	{
		LastPostedStaffCount.Remove(Handle);
	}
}
