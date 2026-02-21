// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSmartObjectPlannerSubsystem.h"

#include "ArcMassGoalPlanInfoSharedFragment.h"
#include "ArcSmartObjectPlanConditionEvaluator.h"
#include "ArcSmartObjectPlanResponse.h"
#include "ArcMass/ArcMassSpatialHashSubsystem.h"
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
#include "SmartObjectSubsystem.h"
#include "InstancedActors/ArcCoreInstancedActorsSubsystem.h"
#include "SmartObjectRequestTypes.h"

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

		TArray<FArcPotentialEntity> Candidates;
		Candidates.Reserve(64);
		GatherCandidates(Request, Candidates);

		BuildAllPlans(Request, Candidates);
	}
}

TStatId UArcSmartObjectPlannerSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArcSmartObjectPlannerSubsystem, STATGROUP_Tickables);
}

void UArcSmartObjectPlannerSubsystem::GatherCandidates(
	const FArcSmartObjectPlanRequest& Request,
	TArray<FArcPotentialEntity>& OutCandidates)
{
	UMassEntitySubsystem* MassEntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	USmartObjectSubsystem* SOSubsystem = GetWorld()->GetSubsystem<USmartObjectSubsystem>();
	const UArcMassSpatialHashSubsystem* SpatialHash = GetWorld()->GetSubsystem<UArcMassSpatialHashSubsystem>();
	if (!MassEntitySubsystem || !SOSubsystem || !SpatialHash)
	{
		return;
	}

	FMassEntityManager& EntityManager = MassEntitySubsystem->GetMutableEntityManager();

	// Query spatial hash for nearby entities, filtering to only those with SmartObject + GoalPlanInfo
	TArray<FArcMassEntityInfo> NearbyEntities;
	NearbyEntities.Reserve(64);

	SpatialHash->GetSpatialHashGrid().QueryEntitiesInRadiusFiltered(
		Request.SearchOrigin,
		Request.SearchRadius,
		[&EntityManager](const FMassEntityHandle& Entity, const FVector& /*Location*/) -> bool
		{
			return EntityManager.GetFragmentDataPtr<FArcSmartObjectOwnerFragment>(Entity) != nullptr
				&& EntityManager.GetConstSharedFragmentDataPtr<FArcMassGoalPlanInfoSharedFragment>(Entity) != nullptr;
		},
		NearbyEntities);

	for (const FArcMassEntityInfo& Info : NearbyEntities)
	{
		const FArcSmartObjectOwnerFragment* SmartObjectOwner = EntityManager.GetFragmentDataPtr<FArcSmartObjectOwnerFragment>(Info.Entity);
		const FArcMassGoalPlanInfoSharedFragment* GoalPlanInfo = EntityManager.GetConstSharedFragmentDataPtr<FArcMassGoalPlanInfoSharedFragment>(Info.Entity);

		// Already filtered above, but guard anyway
		if (!SmartObjectOwner || !GoalPlanInfo)
		{
			continue;
		}

		FArcPotentialEntity PotentialEntity;
		PotentialEntity.RequestingEntity = Request.RequestingEntity;
		PotentialEntity.EntityHandle = Info.Entity;
		PotentialEntity.Location = Info.Location;
		PotentialEntity.Provides = GoalPlanInfo->Provides;
		PotentialEntity.Requires = GoalPlanInfo->Requires;

		for (const auto& Cond : GoalPlanInfo->CustomConditions)
		{
			PotentialEntity.CustomConditions.Add(FConstStructView(Cond.GetScriptStruct(), Cond.GetMemory()));
		}

		TArray<FSmartObjectSlotHandle> OutSlots;
		SOSubsystem->FindSlots(SmartObjectOwner->SmartObjectHandle, FSmartObjectRequestFilter(), /*out*/ OutSlots, {});

		for (int32 SlotIndex = 0; SlotIndex < OutSlots.Num(); SlotIndex++)
		{
			if (!SOSubsystem->CanBeClaimed(OutSlots[SlotIndex]))
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
			CandidateSlot.Result.SlotHandle = OutSlots[SlotIndex];
			PotentialEntity.FoundCandidateSlots.Slots[PotentialEntity.FoundCandidateSlots.NumSlots] = CandidateSlot;
			PotentialEntity.FoundCandidateSlots.NumSlots++;
		}

		OutCandidates.Add(PotentialEntity);
	}
}

// -------------------------------------------------------------------
// Planning Algorithm
// -------------------------------------------------------------------

bool UArcSmartObjectPlannerSubsystem::BuildPlanRecursive(
	FMassEntityManager& EntityManager,
	TArray<FArcPotentialEntity>& AvailableEntities,
	const FGameplayTagContainer& NeededTags,
	FGameplayTagContainer& CurrentTags,
	FGameplayTagContainer& AlreadyProvided,
	TArray<FArcSmartObjectPlanStep>& CurrentPlan,
	TArray<FArcSmartObjectPlanContainer>& OutPlans,
	TArray<bool>& UsedEntities,
	int32 MaxPlans)
{
	// Stop if we already have enough plans
	if (OutPlans.Num() >= MaxPlans)
	{
		return true;
	}

	// Prevent plans that are too long
	if (CurrentPlan.Num() >= 20)
	{
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

	// Find what we still need
	FGameplayTagContainer StillNeeded = NeededTags;
	StillNeeded.RemoveTags(CurrentTags);

	bool bFoundValidPlan = false;

	USmartObjectSubsystem* SmartObjectSubsystem = GetWorld()->GetSubsystem<USmartObjectSubsystem>();

	// Try each available entity
	for (int32 EntityIndex = 0; EntityIndex < AvailableEntities.Num(); EntityIndex++)
	{
		// Skip if already used in this plan
		if (UsedEntities[EntityIndex])
		{
			continue;
		}

		FArcPotentialEntity& Entity = AvailableEntities[EntityIndex];

		bool bHasAnyFreeSlot = false;

		for (const FSmartObjectCandidateSlot& Slot : Entity.FoundCandidateSlots.Slots)
		{
			const bool bCanBeClaimed = SmartObjectSubsystem->CanBeClaimed(Slot.Result.SlotHandle);
			if (bCanBeClaimed)
			{
				bHasAnyFreeSlot = true;
				break;
			}
		}

		if (!bHasAnyFreeSlot)
		{
			continue;
		}

		if (!EvaluateCustomConditions(Entity, EntityManager))
		{
			continue;
		}

		// CRITICAL CHECK: Does this entity provide anything NEW that we don't already have?
		FGameplayTagContainer NewTagsThisEntityWouldAdd = Entity.Provides;
		NewTagsThisEntityWouldAdd.RemoveTags(AlreadyProvided);
		NewTagsThisEntityWouldAdd = NewTagsThisEntityWouldAdd.Filter(StillNeeded);

		if (NewTagsThisEntityWouldAdd.IsEmpty())
		{
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
					EntityManager,
					AvailableEntities,
					MissingRequirements,
					ReqCurrentTags,
					ReqAlreadyProvided,
					ReqCurrentPlan,
					RequirementPlans,
					UsedEntities,
					1);

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
						EntityManager,
						AvailableEntities,
						NeededTags,
						CurrentTags,
						AlreadyProvided,
						CurrentPlan,
						OutPlans,
						UsedEntities,
						MaxPlans))
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
		CurrentPlan.Add(Item);

		// Continue recursively
		if (BuildPlanRecursive(
			EntityManager,
			AvailableEntities,
			NeededTags,
			CurrentTags,
			AlreadyProvided,
			CurrentPlan,
			OutPlans,
			UsedEntities,
			MaxPlans))
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


void UArcSmartObjectPlannerSubsystem::BuildAllPlans(
	const FArcSmartObjectPlanRequest& Request,
	TArray<FArcPotentialEntity>& AvailableEntities)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UArcSmartObjectPlannerSubsystem::BuildAllPlans);

	TArray<bool> UsedEntities;
	UsedEntities.SetNumZeroed(AvailableEntities.Num());

	FArcSmartObjectPlanResponse Response;
	Response.Handle = Request.Handle;
	Response.AccumulatedTags.AppendTags(Request.InitialTags);

	Response.Plans.Reserve(32);

	FMassEntityManager& EntityManager = GetWorld()->GetSubsystem<UMassEntitySubsystem>()->GetMutableEntityManager();

	FGameplayTagContainer CurrentTags = Request.InitialTags;
	FGameplayTagContainer AlreadyProvided = Request.InitialTags;
	TArray<FArcSmartObjectPlanStep> CurrentPlan;

	BuildPlanRecursive(
		EntityManager,
		AvailableEntities,
		Request.Requires,
		CurrentTags,
		AlreadyProvided,
		CurrentPlan,
		Response.Plans,
		UsedEntities,
		Request.MaxPlans
	);

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


bool UArcSmartObjectPlannerSubsystem::EvaluateCustomConditions(const FArcPotentialEntity& Entity, FMassEntityManager& EntityManager) const
{
	for (const FConstStructView& View : Entity.CustomConditions)
	{
		if (View.Get<FArcSmartObjectPlanConditionEvaluator>().CanUseEntity(Entity, EntityManager) == false)
		{
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
}

void FGameplayDebuggerCategory_SmartObjectPlanner::DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	FGameplayDebuggerCategory::DrawData(OwnerPC, CanvasContext);
}
