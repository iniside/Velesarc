// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcGatheringProcessor.h"

#include "ArcMass/Debugger/ArcEconomyTrace.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemData.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "Sim/ArcBuildingFragments.h"
#include "Sim/ArcEconomyNames.h"
#include "Sim/ArcStaffingProcessor.h"
#include "Sim/ArcWorldSimStores.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcGatheringProcessor)

namespace ArcGatheringProcessorPrivate
{
int32 SumStacks(const FArcMassItemStoreFragment& Store)
{
	int32 Total = 0;
	for (const FArcMassReplicatedItem& Item : Store.ReplicatedItems.Items)
	{
		const FArcItemData* Data = Item.ToItem();
		if (Data)
		{
			Total += static_cast<int32>(Data->GetStacks());
		}
	}
	return Total;
}

const TCHAR* GatheringStateName(EArcGatheringState State)
{
	switch (State)
	{
	case EArcGatheringState::Unstaffed:
		return TEXT("Unstaffed");
	case EArcGatheringState::Gathering:
		return TEXT("Gathering");
	case EArcGatheringState::Blocked:
		return TEXT("Blocked");
	}
	return TEXT("Unknown");
}
}

UArcGatheringProcessor::UArcGatheringProcessor()
	: EntityQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteAfter.Add(UArcStaffingProcessor::StaticClass()->GetFName());
}

void UArcGatheringProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddConstSharedRequirement<FArcGatheringConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FArcGatheringStateFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcEmploymentCountFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddSparseRequirement<FArcBuildingOutputStore>(EMassFragmentPresence::All);
	EntityQuery.RegisterWithProcessor(*this);
}

void UArcGatheringProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	int32 UnstaffedBuildings = 0;
	int32 GatheringBuildings = 0;
	int32 BlockedBuildings = 0;
	int32 TotalEmployment = 0;

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, &UnstaffedBuildings, &GatheringBuildings, &BlockedBuildings, &TotalEmployment](FMassExecutionContext& Ctx)
	{
		const FArcGatheringConfigFragment& Config = Ctx.GetConstSharedFragment<FArcGatheringConfigFragment>();
		TArrayView<FArcGatheringStateFragment> GatheringStates = Ctx.GetMutableFragmentView<FArcGatheringStateFragment>();
		const TConstArrayView<FArcEmploymentCountFragment> EmploymentCounts =
			Ctx.GetFragmentView<FArcEmploymentCountFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcGatheringStateFragment& Gathering = GatheringStates[EntityIt];
			const EArcGatheringState OldState = Gathering.State;
			const int32 EmploymentCount = EmploymentCounts[EntityIt].EmploymentCount;
			TotalEmployment += EmploymentCount;

			if (EmploymentCount == 0)
			{
				Gathering.State = EArcGatheringState::Unstaffed;
			}
			else
			{
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
				const FMassEntityView BuildingView(EntityManager, Entity);
				const FArcBuildingOutputStore* OutputStore = BuildingView.GetSparseFragmentDataPtr<FArcBuildingOutputStore>();
				if (OutputStore && ArcGatheringProcessorPrivate::SumStacks(*OutputStore) >= Config.OutputCapacity)
				{
					Gathering.State = EArcGatheringState::Blocked;
				}
				else
				{
					Gathering.State = EArcGatheringState::Gathering;
				}
			}

			if (Gathering.State != OldState)
			{
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
				TRACE_ARC_ECONOMY_BOOKMARKF(
					TEXT("GATHERING-STATE building=%s %s->%s"),
					*ArcEconomyNames::BuildingName(EntityManager, Entity),
					ArcGatheringProcessorPrivate::GatheringStateName(OldState),
					ArcGatheringProcessorPrivate::GatheringStateName(Gathering.State));
			}

			switch (Gathering.State)
			{
			case EArcGatheringState::Unstaffed:
				++UnstaffedBuildings;
				break;
			case EArcGatheringState::Gathering:
				++GatheringBuildings;
				break;
			case EArcGatheringState::Blocked:
				++BlockedBuildings;
				break;
			}
		}
	});

	TRACE_ARC_ECONOMY_COUNTER_INT(TEXT("Gathering"), TEXT("Unstaffed"), UnstaffedBuildings);
	TRACE_ARC_ECONOMY_COUNTER_INT(TEXT("Gathering"), TEXT("Gathering"), GatheringBuildings);
	TRACE_ARC_ECONOMY_COUNTER_INT(TEXT("Gathering"), TEXT("Blocked"), BlockedBuildings);
	TRACE_ARC_ECONOMY_COUNTER_INT(TEXT("Gathering"), TEXT("EmploymentCount"), TotalEmployment);
}
