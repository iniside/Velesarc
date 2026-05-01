// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSmartObjectPlanKnowledgeSensor.h"

#include "SmartObjectPlanner/ArcSmartObjectPlanRequest.h"
#include "SmartObjectPlanner/ArcPotentialEntity.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeQuery.h"
#include "ArcKnowledgeFilter.h"
#include "MassEntityManager.h"
#include "ArcMass/SmartObject/ArcMassSmartObjectFragments.h"
#include "SmartObjectSubsystem.h"
#include "SmartObjectRequestTypes.h"

void FArcSmartObjectPlanKnowledgeSensor::GatherCandidates(
	const FArcSmartObjectPlanEvaluationContext& Context,
	const FArcSmartObjectPlanRequest& Request,
	TArray<FArcPotentialEntity>& OutCandidates) const
{
	const UWorld* World = Context.EntityManager->GetWorld();
	if (!World)
	{
		return;
	}

	UArcKnowledgeSubsystem* KnowledgeSubsystem = World->GetSubsystem<UArcKnowledgeSubsystem>();
	if (!KnowledgeSubsystem)
	{
		return;
	}

	FArcKnowledgeQuery Query;
	Query.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(KnowledgeQueryTags);
	Query.MaxResults = 50;
	Query.SelectionMode = EArcKnowledgeSelectionMode::HighestScore;

	FInstancedStruct DistanceFilter;
	DistanceFilter.InitializeAs<FArcKnowledgeFilter_MaxDistance>();
	DistanceFilter.GetMutable<FArcKnowledgeFilter_MaxDistance>().MaxDistance = Request.SearchRadius;
	Query.Filters.Add(MoveTemp(DistanceFilter));

	FInstancedStruct NotClaimedFilter;
	NotClaimedFilter.InitializeAs<FArcKnowledgeFilter_NotClaimed>();
	Query.Filters.Add(MoveTemp(NotClaimedFilter));

	if (RequiredPayloadType)
	{
		FInstancedStruct PayloadFilter;
		PayloadFilter.InitializeAs<FArcKnowledgeFilter_PayloadType>();
		PayloadFilter.GetMutable<FArcKnowledgeFilter_PayloadType>().RequiredPayloadType = RequiredPayloadType;
		Query.Filters.Add(MoveTemp(PayloadFilter));
	}

	FArcKnowledgeQueryContext QueryContext;
	QueryContext.QueryOrigin = Context.RequestingLocation;
	QueryContext.QuerierEntity = Context.RequestingEntity;
	QueryContext.EntityManager = const_cast<FMassEntityManager*>(Context.EntityManager);
	QueryContext.World = const_cast<UWorld*>(World);
	QueryContext.CurrentTime = World->GetTimeSeconds();

	TArray<FArcKnowledgeQueryResult> Results;
	KnowledgeSubsystem->QueryKnowledge(Query, QueryContext, Results);

	for (const FArcKnowledgeQueryResult& Result : Results)
	{
		const FArcKnowledgeEntry& Entry = Result.Entry;

		FArcPotentialEntity PotentialEntity;
		PotentialEntity.EntityHandle = Entry.SourceEntity;
		PotentialEntity.Location = Entry.Location;
		PotentialEntity.Provides = Entry.Tags;
		PotentialEntity.KnowledgeHandle = Entry.Handle;

		if (Entry.SourceEntity.IsValid())
		{
			const FArcSmartObjectOwnerFragment* SmartObjectOwner = Context.EntityManager->GetFragmentDataPtr<FArcSmartObjectOwnerFragment>(Entry.SourceEntity);
			if (SmartObjectOwner)
			{
				USmartObjectSubsystem* SOSubsystem = World->GetSubsystem<USmartObjectSubsystem>();
				if (SOSubsystem)
				{
					TArray<FSmartObjectSlotHandle> SlotHandles;
					SOSubsystem->FindSlots(SmartObjectOwner->SmartObjectHandle, FSmartObjectRequestFilter(), SlotHandles, {});

					for (int32 SlotIndex = 0; SlotIndex < SlotHandles.Num(); SlotIndex++)
					{
						if (!SOSubsystem->CanBeClaimed(SlotHandles[SlotIndex]))
						{
							continue;
						}

						if (PotentialEntity.FoundCandidateSlots.NumSlots >= 4)
						{
							PotentialEntity.FoundCandidateSlots.NumSlots = 4;
							break;
						}

						FSmartObjectCandidateSlot CandidateSlot;
						CandidateSlot.Result.SmartObjectHandle = SmartObjectOwner->SmartObjectHandle;
						CandidateSlot.Result.SlotHandle = SlotHandles[SlotIndex];
						PotentialEntity.FoundCandidateSlots.Slots[PotentialEntity.FoundCandidateSlots.NumSlots] = CandidateSlot;
						PotentialEntity.FoundCandidateSlots.NumSlots++;
					}

					if (PotentialEntity.FoundCandidateSlots.NumSlots > 0)
					{
						PotentialEntity.KnowledgeHandle = FArcKnowledgeHandle();
					}
				}
			}
		}

		OutCandidates.Add(PotentialEntity);
	}
}
