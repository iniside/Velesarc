// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSmartObjectPlannerSubsystem.h"

#include "ArcSmartObjectPlanConditionEvaluator.h"
#include "ArcSmartObjectPlanResponse.h"
#include "GameplayDebuggerCategoryReplicator.h"
#include "GameplayTagContainer.h"
#include "HAL/PlatformTime.h"
#include "MassActorSubsystem.h"
#include "MassAgentComponent.h"
#include "MassCommonFragments.h"
#include "MassDebugger.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "MassGameplayDebugTypes.h"
#include "InstancedActors/ArcCoreInstancedActorsSubsystem.h"

// -------------------------------------------------------------------
// UArcSmartObjectPlannerSubsystem
// -------------------------------------------------------------------

void UArcSmartObjectPlannerSubsystem::Tick(float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UArcSmartObjectPlannerSubsystem::Tick);

	const double StartTime = FPlatformTime::Seconds();
	const double TimeBudgetSeconds = TimeBudgetMs / 1000.0;

	// Process requests FIFO
	while (!RequestQueue.IsEmpty())
	{
		const double Elapsed = FPlatformTime::Seconds() - StartTime;
		if (Elapsed >= TimeBudgetSeconds)
		{
			break;
		}

		FArcSmartObjectPlanRequest Request = RequestQueue[0];
		RequestQueue.RemoveAt(0);

		if (!Request.IsValid())
		{
			continue;
		}

		BuildAllPlans(Request);
	}
}

TStatId UArcSmartObjectPlannerSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArcSmartObjectPlannerSubsystem, STATGROUP_Tickables);
}

void UArcSmartObjectPlannerSubsystem::RunSensors(
	const FArcSmartObjectPlanRequest& Request,
	const FArcSmartObjectPlanEvaluationContext& Context,
	TArray<FArcPotentialEntity>& OutCandidates)
{
	for (const TInstancedStruct<FArcSmartObjectPlanSensor>& Sensor : Request.SensorsArray)
	{
		if (Sensor.IsValid())
		{
			Sensor.Get().GatherCandidates(Context, Request, OutCandidates);
		}
	}
}

void UArcSmartObjectPlannerSubsystem::DeduplicateCandidates(TArray<FArcPotentialEntity>& Candidates)
{
	for (int32 LaterIdx = Candidates.Num() - 1; LaterIdx > 0; --LaterIdx)
	{
		for (int32 EarlierIdx = 0; EarlierIdx < LaterIdx; ++EarlierIdx)
		{
			if (Candidates[LaterIdx].EntityHandle != Candidates[EarlierIdx].EntityHandle)
			{
				continue;
			}

			// Later sensor's Provides/Requires win
			Candidates[EarlierIdx].Provides = Candidates[LaterIdx].Provides;
			Candidates[EarlierIdx].Requires = Candidates[LaterIdx].Requires;

			// Keep first non-empty FoundCandidateSlots
			if (Candidates[EarlierIdx].FoundCandidateSlots.NumSlots == 0)
			{
				Candidates[EarlierIdx].FoundCandidateSlots = Candidates[LaterIdx].FoundCandidateSlots;
			}

			// Union CustomConditions
			for (const FConstStructView& Cond : Candidates[LaterIdx].CustomConditions)
			{
				Candidates[EarlierIdx].CustomConditions.Add(Cond);
			}

			Candidates.RemoveAtSwap(LaterIdx);
			break;
		}
	}
}

// -------------------------------------------------------------------
// Planning Algorithm
// -------------------------------------------------------------------

bool UArcSmartObjectPlannerSubsystem::BuildPlanRecursive(
	TArray<FArcPotentialEntity>& AvailableEntities,
	const FGameplayTagContainer& NeededTags,
	FGameplayTagContainer& CurrentTags,
	FGameplayTagContainer& AlreadyProvided,
	TArray<FArcSmartObjectPlanStep>& CurrentPlan,
	TArray<FArcSmartObjectPlanContainer>& OutPlans,
	TArray<bool>& UsedEntities,
	int32 MaxPlans,
	const FArcSmartObjectPlanEvaluationContext* Context
#if !UE_BUILD_SHIPPING
	, FArcSmartObjectPlanDebugData* DebugData
#endif
)
{
	// Stop if we already have enough plans
	if (OutPlans.Num() >= MaxPlans)
	{
		return true;
	}

	// Prevent plans that are too long
	if (CurrentPlan.Num() >= 20)
	{
#if !UE_BUILD_SHIPPING
		if (DebugData) DebugData->bHitDepthLimit = true;
#endif
		return false;
	}

	// Base case: if we have everything we need, add this plan
	if (CurrentTags.HasAll(NeededTags))
	{
		FArcSmartObjectPlanContainer NewPlan;
		NewPlan.Items = CurrentPlan;
		OutPlans.Add(NewPlan);
		return true;
	}

#if !UE_BUILD_SHIPPING
	auto SetRejection = [DebugData](FMassEntityHandle Handle, EArcPlanCandidateRejection Reason, const FString& Detail = FString())
	{
		if (!DebugData) return;
		for (FArcPlanCandidateDebugEntry& Entry : DebugData->Candidates)
		{
			if (Entry.EntityHandle == Handle && Entry.Rejection == EArcPlanCandidateRejection::None)
			{
				Entry.Rejection = Reason;
				Entry.RejectionDetail = Detail;
				break;
			}
		}
	};
#endif

	// Find what we still need
	FGameplayTagContainer StillNeeded = NeededTags;
	StillNeeded.RemoveTags(CurrentTags);

	bool bFoundValidPlan = false;

	// Try each available entity
	for (int32 EntityIndex = 0; EntityIndex < AvailableEntities.Num(); EntityIndex++)
	{
		// Skip if already used in this plan
		if (UsedEntities[EntityIndex])
		{
			continue;
		}

		FArcPotentialEntity& Entity = AvailableEntities[EntityIndex];

		// Slot availability already verified by GatherCandidates
		if (Entity.FoundCandidateSlots.NumSlots == 0 && !Entity.KnowledgeHandle.IsValid())
		{
#if !UE_BUILD_SHIPPING
			SetRejection(Entity.EntityHandle, EArcPlanCandidateRejection::NoSlots);
#endif
			continue;
		}

#if !UE_BUILD_SHIPPING
		FString ConditionFailName;
#endif
		if (Context && !EvaluateCustomConditions(Entity, *Context
#if !UE_BUILD_SHIPPING
			, &ConditionFailName
#endif
		))
		{
#if !UE_BUILD_SHIPPING
			SetRejection(Entity.EntityHandle, EArcPlanCandidateRejection::CustomConditionFailed, ConditionFailName);
#endif
			continue;
		}

		// CRITICAL CHECK: Does this entity provide anything NEW that we don't already have?
		FGameplayTagContainer NewTagsThisEntityWouldAdd = Entity.Provides;
		NewTagsThisEntityWouldAdd.RemoveTags(AlreadyProvided);
		NewTagsThisEntityWouldAdd = NewTagsThisEntityWouldAdd.Filter(StillNeeded);

		if (NewTagsThisEntityWouldAdd.IsEmpty())
		{
#if !UE_BUILD_SHIPPING
			SetRejection(Entity.EntityHandle, EArcPlanCandidateRejection::NoNewTags,
				FString::Printf(TEXT("Provides: %s, Already have: %s"),
					*Entity.Provides.ToStringSimple(), *AlreadyProvided.ToStringSimple()));
#endif
			continue; // This entity doesn't add anything new that we need
		}

		// Check if this entity's requirements can be satisfied
		if (!Entity.Requires.IsEmpty())
		{
			FGameplayTagContainer MissingRequirements = Entity.Requires;
			MissingRequirements.RemoveTags(CurrentTags);

			if (!MissingRequirements.IsEmpty())
			{
				// Need to satisfy requirements first — use a separate plan search
				// Reserve the current entity so requirement plan can't use it
				UsedEntities[EntityIndex] = true;

				// Build a sub-plan to satisfy requirements
				TArray<FArcSmartObjectPlanContainer> RequirementPlans;
				FGameplayTagContainer ReqCurrentTags = CurrentTags;
				FGameplayTagContainer ReqAlreadyProvided = AlreadyProvided;
				TArray<FArcSmartObjectPlanStep> ReqCurrentPlan = CurrentPlan;

				const bool bFoundReqPlan = BuildPlanRecursive(
					AvailableEntities,
					MissingRequirements,
					ReqCurrentTags,
					ReqAlreadyProvided,
					ReqCurrentPlan,
					RequirementPlans,
					UsedEntities,
					1,
					Context
#if !UE_BUILD_SHIPPING
					, DebugData
#endif
				);

				UsedEntities[EntityIndex] = false; // Unreserve

				if (bFoundReqPlan && !RequirementPlans.IsEmpty())
				{
					const FArcSmartObjectPlanContainer& RequirementPlan = RequirementPlans[0];

					// Calculate what the requirement plan provides
					FGameplayTagContainer RequirementPlanProvides;
					for (const FArcSmartObjectPlanStep& ReqItem : RequirementPlan.Items)
					{
						for (const FArcPotentialEntity& ReqEntity : AvailableEntities)
						{
							if (ReqEntity.EntityHandle == ReqItem.EntityHandle)
							{
								RequirementPlanProvides.AppendTags(ReqEntity.Provides);
								break;
							}
						}
					}

					// DUPLICATE CHECK: Does requirement plan provide capabilities we already have?
					FGameplayTagContainer ConflictingTags = RequirementPlanProvides.Filter(AlreadyProvided);

					if (!ConflictingTags.IsEmpty())
					{
#if !UE_BUILD_SHIPPING
						SetRejection(Entity.EntityHandle, EArcPlanCandidateRejection::RequirementConflict,
							FString::Printf(TEXT("Conflicting: %s"), *ConflictingTags.ToStringSimple()));
#endif
						continue; // Requirement plan would introduce duplicate capabilities
					}

					// PUSH: requirement plan steps + current entity
					const int32 PlanSizeBefore = CurrentPlan.Num();
					CurrentPlan.Append(RequirementPlan.Items);

					FArcSmartObjectPlanStep Item;
					Item.EntityHandle = Entity.EntityHandle;
					Item.Location = Entity.Location;
					Item.Requires = Entity.Requires;
					Item.FoundCandidateSlots = Entity.FoundCandidateSlots;
					Item.KnowledgeHandle = Entity.KnowledgeHandle;
					CurrentPlan.Add(Item);

					CurrentTags.AppendTags(RequirementPlanProvides);
					CurrentTags.AppendTags(Entity.Provides);
					AlreadyProvided.AppendTags(RequirementPlanProvides);
					AlreadyProvided.AppendTags(Entity.Provides);

					// Mark requirement plan entities as used
					for (const FArcSmartObjectPlanStep& ReqItem : RequirementPlan.Items)
					{
						for (int32 i = 0; i < AvailableEntities.Num(); i++)
						{
							if (AvailableEntities[i].EntityHandle == ReqItem.EntityHandle)
							{
								UsedEntities[i] = true;
								break;
							}
						}
					}
					UsedEntities[EntityIndex] = true;

					// Continue with the complete plan
					if (BuildPlanRecursive(
						AvailableEntities,
						NeededTags,
						CurrentTags,
						AlreadyProvided,
						CurrentPlan,
						OutPlans,
						UsedEntities,
						MaxPlans,
						Context
#if !UE_BUILD_SHIPPING
						, DebugData
#endif
					))
					{
						bFoundValidPlan = true;
					}

					// POP: undo everything
					UsedEntities[EntityIndex] = false;
					for (const FArcSmartObjectPlanStep& ReqItem : RequirementPlan.Items)
					{
						for (int32 i = 0; i < AvailableEntities.Num(); i++)
						{
							if (AvailableEntities[i].EntityHandle == ReqItem.EntityHandle)
							{
								UsedEntities[i] = false;
								break;
							}
						}
					}

					AlreadyProvided.RemoveTags(Entity.Provides);
					AlreadyProvided.RemoveTags(RequirementPlanProvides);
					CurrentTags.RemoveTags(Entity.Provides);
					CurrentTags.RemoveTags(RequirementPlanProvides);

					CurrentPlan.SetNum(PlanSizeBefore);
				}
#if !UE_BUILD_SHIPPING
				else
				{
					SetRejection(Entity.EntityHandle, EArcPlanCandidateRejection::RequirementUnsatisfiable,
						FString::Printf(TEXT("Missing: %s"), *MissingRequirements.ToStringSimple()));
				}
#endif

				// Don't continue to direct use if we had missing requirements
				continue;
			}
		}

		// DIRECT USE: Entity has no requirements or all requirements are already satisfied
		// PUSH
		UsedEntities[EntityIndex] = true;
		CurrentTags.AppendTags(Entity.Provides);
		AlreadyProvided.AppendTags(Entity.Provides);

		FArcSmartObjectPlanStep Item;
		Item.EntityHandle = Entity.EntityHandle;
		Item.Location = Entity.Location;
		Item.Requires = Entity.Requires;
		Item.FoundCandidateSlots = Entity.FoundCandidateSlots;
		Item.KnowledgeHandle = Entity.KnowledgeHandle;
		CurrentPlan.Add(Item);

		// Continue recursively
		if (BuildPlanRecursive(
			AvailableEntities,
			NeededTags,
			CurrentTags,
			AlreadyProvided,
			CurrentPlan,
			OutPlans,
			UsedEntities,
			MaxPlans,
			Context
#if !UE_BUILD_SHIPPING
			, DebugData
#endif
		))
		{
			bFoundValidPlan = true;
		}

		// POP
		CurrentPlan.Pop();
		AlreadyProvided.RemoveTags(Entity.Provides);
		CurrentTags.RemoveTags(Entity.Provides);
		UsedEntities[EntityIndex] = false;

		// Stop if we have enough plans
		if (OutPlans.Num() >= MaxPlans)
		{
			break;
		}
	}

	return bFoundValidPlan;
}


void UArcSmartObjectPlannerSubsystem::BuildAllPlans(const FArcSmartObjectPlanRequest& Request)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UArcSmartObjectPlannerSubsystem::BuildAllPlans);

	FMassEntityManager& EntityManager = GetWorld()->GetSubsystem<UMassEntitySubsystem>()->GetMutableEntityManager();

	FArcSmartObjectPlanEvaluationContext Context;
	Context.RequestingEntity = Request.RequestingEntity;
	Context.EntityManager = &EntityManager;
	if (const FTransformFragment* Transform = EntityManager.GetFragmentDataPtr<FTransformFragment>(Request.RequestingEntity))
	{
		Context.RequestingLocation = Transform->GetTransform().GetLocation();
	}

	TArray<FArcPotentialEntity> AvailableEntities;
	AvailableEntities.Reserve(64);

#if !UE_BUILD_SHIPPING
	TMap<FMassEntityHandle, FString> EntitySensorSources;

	for (const TInstancedStruct<FArcSmartObjectPlanSensor>& Sensor : Request.SensorsArray)
	{
		if (!Sensor.IsValid())
		{
			continue;
		}

		const int32 CountBefore = AvailableEntities.Num();
		Sensor.Get().GatherCandidates(Context, Request, AvailableEntities);

		FString SensorName = Sensor.GetScriptStruct()->GetDisplayNameText().ToString();
		for (int32 i = CountBefore; i < AvailableEntities.Num(); i++)
		{
			FString& Existing = EntitySensorSources.FindOrAdd(AvailableEntities[i].EntityHandle);
			if (Existing.IsEmpty())
			{
				Existing = SensorName;
			}
			else
			{
				Existing += TEXT(", ");
				Existing += SensorName;
			}
		}
	}
#else
	RunSensors(Request, Context, AvailableEntities);
#endif

	DeduplicateCandidates(AvailableEntities);

#if !UE_BUILD_SHIPPING
	FArcSmartObjectPlanDebugData DebugData;
	DebugData.SearchOrigin = Request.SearchOrigin;
	DebugData.SearchRadius = Request.SearchRadius;
	DebugData.RequiredTags = Request.Requires;
	DebugData.InitialTags = Request.InitialTags;

	DebugData.Candidates.Reserve(AvailableEntities.Num());
	for (const FArcPotentialEntity& Entity : AvailableEntities)
	{
		FArcPlanCandidateDebugEntry Entry;
		Entry.EntityHandle = Entity.EntityHandle;
		Entry.Location = Entity.Location;
		Entry.Provides = Entity.Provides;
		Entry.Requires = Entity.Requires;
		Entry.SlotCount = Entity.FoundCandidateSlots.NumSlots;
		Entry.SensorSource = EntitySensorSources.FindRef(Entity.EntityHandle);
		DebugData.Candidates.Add(Entry);
	}
#endif

	TArray<bool> UsedEntities;
	UsedEntities.SetNumZeroed(AvailableEntities.Num());

	FArcSmartObjectPlanResponse Response;
	Response.Handle = Request.Handle;
	Response.AccumulatedTags.AppendTags(Request.InitialTags);

	Response.Plans.Reserve(32);

	FGameplayTagContainer CurrentTags = Request.InitialTags;
	FGameplayTagContainer AlreadyProvided = Request.InitialTags;
	TArray<FArcSmartObjectPlanStep> CurrentPlan;

	BuildPlanRecursive(
		AvailableEntities,
		Request.Requires,
		CurrentTags,
		AlreadyProvided,
		CurrentPlan,
		Response.Plans,
		UsedEntities,
		Request.MaxPlans,
		&Context
#if !UE_BUILD_SHIPPING
		, &DebugData
#endif
	);

#if !UE_BUILD_SHIPPING
	DebugData.PlansFound = Response.Plans.Num();

	FGameplayTagContainer AllProvided;
	for (const FArcPotentialEntity& Entity : AvailableEntities)
	{
		AllProvided.AppendTags(Entity.Provides);
	}
	DebugData.UnsatisfiedTags = Request.Requires;
	DebugData.UnsatisfiedTags.RemoveTags(AllProvided);
	DebugData.UnsatisfiedTags.RemoveTags(Request.InitialTags);

	SetDebugDiagnostics(Request.RequestingEntity, MoveTemp(DebugData));
#endif

	// Sort plans by efficiency
	Response.Plans.Sort([&Request](const FArcSmartObjectPlanContainer& A, const FArcSmartObjectPlanContainer& B)
	{
		// Prefer plans with fewer steps
		if (A.Items.Num() != B.Items.Num())
		{
			return A.Items.Num() < B.Items.Num();
		}

		// Secondary sort by total travel distance
		auto CalcTravelDistance = [&Request](const TArray<FArcSmartObjectPlanStep>& Items) -> float
		{
			float Total = 0.0f;
			FVector Prev = Request.SearchOrigin;
			for (const FArcSmartObjectPlanStep& Item : Items)
			{
				Total += FVector::Dist(Prev, Item.Location);
				Prev = Item.Location;
			}
			return Total;
		};

		return CalcTravelDistance(A.Items) < CalcTravelDistance(B.Items);
	});

	Request.FinishedDelegate.ExecuteIfBound(Response);
}


bool UArcSmartObjectPlannerSubsystem::EvaluateCustomConditions(
	const FArcPotentialEntity& Entity,
	const FArcSmartObjectPlanEvaluationContext& Context
#if !UE_BUILD_SHIPPING
	, FString* OutFailedConditionName
#endif
)
{
	for (const FConstStructView& View : Entity.CustomConditions)
	{
		if (View.Get<FArcSmartObjectPlanConditionEvaluator>().CanUseEntity(Entity, Context) == false)
		{
#if !UE_BUILD_SHIPPING
			if (OutFailedConditionName)
			{
				*OutFailedConditionName = View.GetScriptStruct()->GetName();
			}
#endif
			return false;
		}
	}

	return true;
}

// -------------------------------------------------------------------
// Gameplay Debugger
// -------------------------------------------------------------------

namespace Arcx
{
	FMassEntityHandle GetEntityFromActor(const AActor& Actor, const UMassAgentComponent*& OutMassAgentComponent)
	{
		FMassEntityHandle EntityHandle;
		if (const UMassAgentComponent* AgentComp = Actor.FindComponentByClass<UMassAgentComponent>())
		{
			EntityHandle = AgentComp->GetEntityHandle();
			OutMassAgentComponent = AgentComp;
		}
		else if (UMassActorSubsystem* ActorSubsystem = UWorld::GetSubsystem<UMassActorSubsystem>(Actor.GetWorld()))
		{
			EntityHandle = ActorSubsystem->GetEntityHandleFromActor(&Actor);
		}
		return EntityHandle;
	};

	FMassEntityHandle GetBestEntity(const FVector ViewLocation, const FVector ViewDirection, const TConstArrayView<FMassEntityHandle> Entities
		, const TConstArrayView<FVector> Locations, const bool bLimitAngle, const FVector::FReal MaxScanDistance)
	{
		constexpr FVector::FReal MinViewDirDot = 0.707; // 45 degrees
		const FVector::FReal MaxScanDistanceSq = MaxScanDistance * MaxScanDistance;

		checkf(Entities.Num() == Locations.Num(), TEXT("Both Entities and Locations lists are expected to be of the same size: %d vs %d"), Entities.Num(), Locations.Num());

		FVector::FReal BestScore = bLimitAngle ? MinViewDirDot : (-1. - KINDA_SMALL_NUMBER);
		FMassEntityHandle BestEntity;

		for (int i = 0; i < Entities.Num(); ++i)
		{
			if (Entities[i].IsSet() == false)
			{
				continue;
			}

			const FVector DirToEntity = (Locations[i] - ViewLocation);
			const FVector::FReal DistToEntitySq = DirToEntity.SizeSquared();
			if (DistToEntitySq > MaxScanDistanceSq)
			{
				continue;
			}

			const FVector::FReal Distance = FMath::Sqrt(DistToEntitySq);
			const FVector DirToEntityNormal = (FMath::IsNearlyZero(DistToEntitySq)) ? ViewDirection : (DirToEntity / Distance);
			const FVector::FReal ViewDot = FVector::DotProduct(ViewDirection, DirToEntityNormal);
			const FVector::FReal Score = ViewDot * 0.1 * (1. - Distance / MaxScanDistance);
			if (ViewDot > BestScore)
			{
				BestScore = ViewDot;
				BestEntity = Entities[i];
			}
		}

		return BestEntity;
	}
} // namespace Arcx

FGameplayDebuggerCategory_SmartObjectPlanner::FGameplayDebuggerCategory_SmartObjectPlanner()
	: Super()
{
	bPickEntity = false;

	BindKeyPress(EKeys::P.GetFName(), FGameplayDebuggerInputModifier::Shift, this, &FGameplayDebuggerCategory_SmartObjectPlanner::OnPickEntity, EGameplayDebuggerInputMode::Replicated);

	OnEntitySelectedHandle = FMassDebugger::OnEntitySelectedDelegate.AddRaw(this, &FGameplayDebuggerCategory_SmartObjectPlanner::OnEntitySelected);
}

FGameplayDebuggerCategory_SmartObjectPlanner::~FGameplayDebuggerCategory_SmartObjectPlanner()
{
	FMassDebugger::OnEntitySelectedDelegate.Remove(OnEntitySelectedHandle);
}

void FGameplayDebuggerCategory_SmartObjectPlanner::OnEntitySelected(const FMassEntityManager& EntityManager, const FMassEntityHandle EntityHandle)
{
	UWorld* World = EntityManager.GetWorld();
	if (World != GetWorldFromReplicator())
	{
		// ignore, this call is for a different world
		return;
	}

	AActor* BestActor = nullptr;
	if (EntityHandle.IsSet() && World)
	{
		if (const UMassActorSubsystem* ActorSubsystem = World->GetSubsystem<UMassActorSubsystem>())
		{
			BestActor = ActorSubsystem->GetActorFromHandle(EntityHandle);
		}
	}

	CachedEntity = EntityHandle;
	CachedDebugActor = BestActor;
	check(GetReplicator());
	GetReplicator()->SetDebugActor(BestActor);
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_SmartObjectPlanner::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerCategory_SmartObjectPlanner());
}

void FGameplayDebuggerCategory_SmartObjectPlanner::SetCachedEntity(const FMassEntityHandle Entity, const FMassEntityManager& EntityManager)
{
	if (CachedEntity != Entity)
	{
		FMassDebugger::SelectEntity(EntityManager, Entity);
	}
}

void FGameplayDebuggerCategory_SmartObjectPlanner::PickEntity(const FVector& ViewLocation, const FVector& ViewDirection, const UWorld& World, FMassEntityManager& EntityManager, const bool bLimitAngle)
{
	FMassEntityHandle BestEntity;
	float SearchRange = 25000.f;
	// entities indicated by UE::Mass::Debug take precedence
	if (UE::Mass::Debug::HasDebugEntities() && !UE::Mass::Debug::IsDebuggingSingleEntity())
	{
		TArray<FMassEntityHandle> Entities;
		TArray<FVector> Locations;
		UE::Mass::Debug::GetDebugEntitiesAndLocations(EntityManager, Entities, Locations);
		BestEntity = Arcx::GetBestEntity(ViewLocation, ViewDirection, Entities, Locations, bLimitAngle, SearchRange);
	}
	else
	{
		TArray<FMassEntityHandle> Entities;
		TArray<FVector> Locations;
		FMassExecutionContext ExecutionContext(EntityManager);
		FMassEntityQuery Query(EntityManager.AsShared());
		Query.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
		Query.ForEachEntityChunk(ExecutionContext, [&Entities, &Locations](FMassExecutionContext& Context)
		{
			Entities.Append(Context.GetEntities().GetData(), Context.GetEntities().Num());
			TConstArrayView<FTransformFragment> InLocations = Context.GetFragmentView<FTransformFragment>();
			Locations.Reserve(Locations.Num() + InLocations.Num());
			for (const FTransformFragment& TransformFragment : InLocations)
			{
				Locations.Add(TransformFragment.GetTransform().GetLocation());
			}
		});

		BestEntity = Arcx::GetBestEntity(ViewLocation, ViewDirection, Entities, Locations, bLimitAngle, SearchRange);
	}

	SetCachedEntity(BestEntity, EntityManager);
}

void FGameplayDebuggerCategory_SmartObjectPlanner::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	const UWorld* World = GetDataWorld(OwnerPC, DebugActor);
	if (!World)
	{
		return;
	}

	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(World);
	UArcSmartObjectPlannerSubsystem* SmartObjectPlanner = UWorld::GetSubsystem<UArcSmartObjectPlannerSubsystem>(World);

	if (EntitySubsystem == nullptr || SmartObjectPlanner == nullptr)
	{
		AddTextLine(FString::Printf(TEXT("{Red}EntitySubsystem instance is missing")));
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	const UMassAgentComponent* AgentComp = nullptr;
	if (DebugActor)
	{
		const FMassEntityHandle EntityHandle = Arcx::GetEntityFromActor(*DebugActor, AgentComp);
		SetCachedEntity(EntityHandle, EntityManager);
		CachedDebugActor = DebugActor;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewDirection = FVector::ForwardVector;
	if (GetViewPoint(OwnerPC, ViewLocation, ViewDirection))
	{
		if (bPickEntity)
		{
			PickEntity(ViewLocation, ViewDirection, *World, EntityManager);
			bPickEntity = false;
		}
	}

	if (!CachedEntity.IsSet())
	{
		return;
	}

	const FArcSmartObjectPlanContainer* Plan = SmartObjectPlanner->GetDebugPlan(CachedEntity);
	if (!Plan)
	{
		return;
	}

	int32 Num = 0;
	for (const auto& Step : Plan->Items)
	{
		FString Desc = FString::Printf(TEXT("Step Num %d"), Num);
		AddShape(FGameplayDebuggerShape::MakeCylinder(Step.Location, 25.f, 240.f, FColor::Red, Desc));
		Num++;
	}

	for (int32 StepNum = 0; StepNum < Plan->Items.Num(); StepNum++)
	{
		const FArcSmartObjectPlanStep& Step = Plan->Items[StepNum];

		int32 NextStepNum = StepNum + 1;
		if (!Plan->Items.IsValidIndex(NextStepNum))
		{
			break;
		}

		const FArcSmartObjectPlanStep& NextStep = Plan->Items[NextStepNum];

		FVector Start = Step.Location + FVector(0.f, 0.f, 100.f);
		FVector End = NextStep.Location + FVector(0.f, 0.f, 100.f);

		AddShape(FGameplayDebuggerShape::MakeArrow(Start, End, 20.f, 5.f, FColor::Red, FString::Printf(TEXT("Step %d To %d"), StepNum, NextStepNum)));
	}

#if !UE_BUILD_SHIPPING
	const FArcSmartObjectPlanDebugData* Diag = SmartObjectPlanner->GetDebugDiagnostics(CachedEntity);
	if (Diag)
	{
		for (const FArcPlanCandidateDebugEntry& Cand : Diag->Candidates)
		{
			FColor CandColor;
			switch (Cand.Rejection)
			{
			case EArcPlanCandidateRejection::None: CandColor = FColor::Green; break;
			case EArcPlanCandidateRejection::NoSlots: CandColor = FColor(180, 50, 50); break;
			case EArcPlanCandidateRejection::CustomConditionFailed: CandColor = FColor(200, 80, 80); break;
			case EArcPlanCandidateRejection::NoNewTags: CandColor = FColor::Yellow; break;
			case EArcPlanCandidateRejection::RequirementConflict: CandColor = FColor::Orange; break;
			case EArcPlanCandidateRejection::RequirementUnsatisfiable: CandColor = FColor(255, 60, 60); break;
			default: CandColor = FColor::White; break;
			}

			FString Label;
			switch (Cand.Rejection)
			{
			case EArcPlanCandidateRejection::None: Label = TEXT("OK"); break;
			case EArcPlanCandidateRejection::NoSlots: Label = TEXT("No Slots"); break;
			case EArcPlanCandidateRejection::CustomConditionFailed: Label = FString::Printf(TEXT("Cond: %s"), *Cand.RejectionDetail); break;
			case EArcPlanCandidateRejection::NoNewTags: Label = TEXT("No New Tags"); break;
			case EArcPlanCandidateRejection::RequirementConflict: Label = TEXT("Req Conflict"); break;
			case EArcPlanCandidateRejection::RequirementUnsatisfiable: Label = TEXT("Req Unsat"); break;
			default: Label = TEXT("?"); break;
			}

			if (!Cand.SensorSource.IsEmpty())
			{
				Label += FString::Printf(TEXT("\n(%s)"), *Cand.SensorSource);
			}

			AddShape(FGameplayDebuggerShape::MakeCylinder(Cand.Location, 15.f, 180.f, CandColor, Label));
		}
	}
#endif
}

void FGameplayDebuggerCategory_SmartObjectPlanner::DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	FGameplayDebuggerCategory::DrawData(OwnerPC, CanvasContext);
}
