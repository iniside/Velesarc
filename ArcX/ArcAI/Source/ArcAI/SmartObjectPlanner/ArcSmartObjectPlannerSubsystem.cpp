// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSmartObjectPlannerSubsystem.h"

#include "ArcSmartObjectPlanContainer.h"
#include "ArcSmartObjectPlanConditionEvaluator.h"
#include "ArcSmartObjectPlanResponse.h"
#include "GameplayDebuggerCategoryReplicator.h"
#include "GameplayTagContainer.h"
#include "MassActorSubsystem.h"
#include "MassAgentComponent.h"
#include "MassDebugger.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "MassGameplayDebugTypes.h"
#include "SmartObjectSubsystem.h"

#define ARC_PRINT_DEBUG_INFO 0

bool UArcSmartObjectPlannerSubsystem::BuildPlanRecursive(
	FMassEntityManager& EntityManager,
	TArray<FArcPotentialEntity>& AvailableEntities,
	const FGameplayTagContainer& NeededTags,
	FGameplayTagContainer CurrentTags,
	TArray<FArcSmartObjectPlanStep> CurrentPlan,
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
	
	// Calculate what the current plan already provides
	FGameplayTagContainer AlreadyProvidedByPlan = CurrentTags; // Include initial tags
	for (const FArcSmartObjectPlanStep& ExistingItem : CurrentPlan)
	{
		for (const FArcPotentialEntity& ExistingEntity : AvailableEntities)
		{
			if (ExistingEntity.EntityHandle == ExistingItem.EntityHandle)
			{
				AlreadyProvidedByPlan.AppendTags(ExistingEntity.Provides);
				break;
			}
		}
	}
	
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
		NewTagsThisEntityWouldAdd.RemoveTags(AlreadyProvidedByPlan);
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
#if ARC_PRINT_DEBUG_INFO
			FString TagNumberLog;
			TagNumberLog += "Entity ";
			TagNumberLog += Entity.EntityHandle.DebugGetDescription();
			TagNumberLog += "\n";
			TagNumberLog += " Entity Tags: ";
			for (const TPair<FGameplayTag, int32>& TagNumPair : Entity.OwnerTagCount)
			{
				TagNumberLog += "\n";
				TagNumberLog += "  ";
				TagNumberLog += TagNumPair.Key.ToString();
				TagNumberLog += " ";
				TagNumberLog += FString::FromInt(TagNumPair.Value);
			}
			TagNumberLog += "\n";
			
			TagNumberLog += " Accumulated Tags ";
			
			for (const TPair<FGameplayTag, int32>& TagNumPair : AccumulatedTagCount)
			{
				TagNumberLog += "\n";
				TagNumberLog += "  ";
				TagNumberLog += TagNumPair.Key.ToString();
				TagNumberLog += " ";
				TagNumberLog += FString::FromInt(TagNumPair.Value);
			}
			TagNumberLog += "\n";
#endif
			if (!MissingRequirements.IsEmpty())
			{
#if ARC_PRINT_DEBUG_INFO
				TagNumberLog += " Required Tag Num ";
				TagNumberLog += "\n";
#endif

#if ARC_PRINT_DEBUG_INFO
				UE_LOG(LogGoalPlanner, Verbose, TEXT("%s"), *TagNumberLog);
#endif
				// Need to satisfy requirements first
				// IMPORTANT: Pass the current plan's provided tags to requirement resolution
				// so it knows what capabilities we already have
				
				TArray<bool> TempUsedEntities = UsedEntities;
				TempUsedEntities[EntityIndex] = true; // Reserve current entity
				
				// Recursively satisfy requirements, but with awareness of current capabilities
				TArray<FArcSmartObjectPlanContainer> RequirementPlans;
				if (BuildPlanRecursive(
					EntityManager,
					AvailableEntities,
					MissingRequirements,
					CurrentTags, // Current state, not including what we already provide in plan
					CurrentPlan, // Pass current plan so requirements resolution knows what we have
					RequirementPlans,
					TempUsedEntities,
					1))
				{
					if (!RequirementPlans.IsEmpty())
					{
						// Check if requirement plan introduces duplicate capabilities
						const FArcSmartObjectPlanContainer& RequirementPlan = RequirementPlans[0];
						
						// Calculate what the requirement plan would provide
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
						//FGameplayTagContainer ConflictingTags = RequirementPlanProvides;
						FGameplayTagContainer ConflictingTags = RequirementPlanProvides.Filter(AlreadyProvidedByPlan);
						
						if (!ConflictingTags.IsEmpty())
						{
							continue; // Requirement plan would introduce duplicate capabilities
						}
						
						// Build complete plan: current plan + requirement plan + current entity
						TArray<FArcSmartObjectPlanStep> CompletePlan = CurrentPlan;
						CompletePlan.Append(RequirementPlan.Items);
						
						FArcSmartObjectPlanStep Item;
						Item.EntityHandle = Entity.EntityHandle;
						Item.Location = Entity.Location;
						Item.Requires = Entity.Requires;
						Item.FoundCandidateSlots = Entity.FoundCandidateSlots;
						
						CompletePlan.Add(Item);
						
						// Calculate new state
						FGameplayTagContainer NewCurrentTags = CurrentTags;
						NewCurrentTags.AppendTags(RequirementPlanProvides);
						NewCurrentTags.AppendTags(Entity.Provides);
						
						// Update used entities
						TArray<bool> NewUsedEntities = UsedEntities;
						for (const FArcSmartObjectPlanStep& ReqItem : RequirementPlan.Items)
						{
							for (int32 i = 0; i < AvailableEntities.Num(); i++)
							{
								if (AvailableEntities[i].EntityHandle == ReqItem.EntityHandle)
								{
									NewUsedEntities[i] = true;
									break;
								}
							}
						}
						NewUsedEntities[EntityIndex] = true;
						
						// Continue with the complete plan
						if (BuildPlanRecursive(
							EntityManager,
							AvailableEntities,
							NeededTags,
							NewCurrentTags,
							CompletePlan,
							OutPlans,
							NewUsedEntities,
							MaxPlans))
						{
							bFoundValidPlan = true;
						}
					}
				}
				
				// Don't continue to direct use if we had missing requirements
				continue;
			}
		}
		
		// DIRECT USE: Entity has no requirements or all requirements are satisfied
		TArray<bool> NewUsedEntities = UsedEntities;
		FGameplayTagContainer NewCurrentTags = CurrentTags;
		TArray<FArcSmartObjectPlanStep> NewCurrentPlan = CurrentPlan;
		
		// Add this entity to the plan
		NewUsedEntities[EntityIndex] = true;
		NewCurrentTags.AppendTags(Entity.Provides);
		
		FArcSmartObjectPlanStep Item;
		Item.EntityHandle = Entity.EntityHandle;
		Item.Location = Entity.Location;
		Item.Requires = Entity.Requires;
		Item.FoundCandidateSlots = Entity.FoundCandidateSlots;
		NewCurrentPlan.Add(Item);
		
		// Continue recursively
		if (BuildPlanRecursive(
			EntityManager,
			AvailableEntities,
			NeededTags,
			NewCurrentTags,
			NewCurrentPlan,
			OutPlans,
			NewUsedEntities,
			MaxPlans))
		{
			bFoundValidPlan = true;
		}
		
		// Stop if we have enough plans
		if (OutPlans.Num() >= MaxPlans)
		{
			break;
		}
	}
	
	return bFoundValidPlan;
}


void UArcSmartObjectPlannerSubsystem::BuildAllPlans(
	FArcSmartObjectPlanRequest Request,
	TArray<FArcPotentialEntity>& AvailableEntities)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UArcSmartObjectPlannerSubsystem::BuildAllPlans);
	// Reset all entities
	for (FArcPotentialEntity& Entity : AvailableEntities)
	{
		Entity.bUsedInPlan = false;
	}
	
	TArray<bool> UsedEntities;
	UsedEntities.SetNumZeroed(AvailableEntities.Num());

	
	FArcSmartObjectPlanResponse Response;
	Response.Handle = Request.Handle;
	Response.AccumulatedTags.AppendTags(Request.InitialTags);
	
	Response.Plans.Reserve(32);
	
	TArray<FArcSmartObjectPlanStep> EmptyPlan;

	FMassEntityManager& EntityManager = GetWorld()->GetSubsystem<UMassEntitySubsystem>()->GetMutableEntityManager();
	// Build plans recursively - now only valid plans are generated
	BuildPlanRecursive(
		EntityManager,
		AvailableEntities,
		Request.Requires,
		Request.InitialTags,
		EmptyPlan,
		Response.Plans,
		UsedEntities,
		Request.MaxPlans
	);
	
	// Minimal validation (should not be needed anymore, but kept for safety)
	TArray<FArcSmartObjectPlanContainer> FinalPlans;
	for (const FArcSmartObjectPlanContainer& Plan : Response.Plans)
	{
		if (!Plan.Items.IsEmpty())
		{
			FinalPlans.Add(Plan);
		}
	}
	
	Response.Plans = FinalPlans;
	
	// Sort plans by efficiency
	Response.Plans.Sort([](const FArcSmartObjectPlanContainer& A, const FArcSmartObjectPlanContainer& B)
	{
		// Prefer plans with fewer steps
		if (A.Items.Num() != B.Items.Num())
		{
			return A.Items.Num() < B.Items.Num();
		}
		
		// Secondary sort by total distance
		float TotalDistanceA = 0.0f;
		float TotalDistanceB = 0.0f;
		
		for (const FArcSmartObjectPlanStep& Item : A.Items)
		{
			TotalDistanceA += Item.Location.Size();
		}
		
		for (const FArcSmartObjectPlanStep& Item : B.Items)
		{
			TotalDistanceB += Item.Location.Size();
		}
		
		return TotalDistanceA < TotalDistanceB;
	});

	Request.FinishedDelegate.ExecuteIfBound(Response);
}


bool UArcSmartObjectPlannerSubsystem::ArePlansIdentical(
	const FArcSmartObjectPlanContainer& PlanA, 
	const FArcSmartObjectPlanContainer& PlanB)
{
	if (PlanA.Items.Num() != PlanB.Items.Num())
	{
		return false;
	}
	
	for (int32 i = 0; i < PlanA.Items.Num(); i++)
	{
		if (PlanA.Items[i].EntityHandle != PlanB.Items[i].EntityHandle)
		{
			return false;
		}
	}
	
	return true;
}

bool UArcSmartObjectPlannerSubsystem::HasCircularDependency(
	const TArray<FArcPotentialEntity>& AvailableEntities,
	const FArcPotentialEntity& StartEntity,
	const TArray<bool>& UsedEntities,
	int32 MaxDepth)
{
	TSet<FMassEntityHandle> VisitedEntities;
	TSet<FMassEntityHandle> CurrentPath;
	
	return HasCircularDependencyRecursive(
		AvailableEntities, 
		StartEntity, 
		VisitedEntities, 
		CurrentPath, 
		UsedEntities,
		0, 
		MaxDepth
	);
}

bool UArcSmartObjectPlannerSubsystem::HasCircularDependencyRecursive(
	const TArray<FArcPotentialEntity>& AvailableEntities,
	const FArcPotentialEntity& CurrentEntity,
	TSet<FMassEntityHandle>& VisitedEntities,
	TSet<FMassEntityHandle>& CurrentPath,
	const TArray<bool>& UsedEntities,
	int32 CurrentDepth,
	int32 MaxDepth)
{
	// Prevent infinite recursion
	if (CurrentDepth > MaxDepth)
	{
		return true; // Assume cycle if too deep
	}
	
	// If we've seen this entity in the current path, it's a cycle
	if (CurrentPath.Contains(CurrentEntity.EntityHandle))
	{
		return true;
	}
	
	// If we've fully processed this entity before, no cycle from here
	if (VisitedEntities.Contains(CurrentEntity.EntityHandle))
	{
		return false;
	}
	
	// Add to current path
	CurrentPath.Add(CurrentEntity.EntityHandle);
	
	// Check all entities that this entity depends on
	for (const FGameplayTag& RequiredTag : CurrentEntity.Requires.GetGameplayTagArray())
	{
		// Find entities that provide this required tag
		for (int32 i = 0; i < AvailableEntities.Num(); i++)
		{
			const FArcPotentialEntity& PotentialProvider = AvailableEntities[i];
			
			// Skip if this entity is already used in the current plan
			if (UsedEntities[i])
			{
				continue;
			}
			
			// Skip self-references
			if (PotentialProvider.EntityHandle == CurrentEntity.EntityHandle)
			{
				continue;
			}
			
			// Check if this entity provides the required tag
			if (PotentialProvider.Provides.HasTag(RequiredTag))
			{
				// Recursively check this provider for cycles
				if (HasCircularDependencyRecursive(
					AvailableEntities,
					PotentialProvider,
					VisitedEntities,
					CurrentPath,
					UsedEntities,
					CurrentDepth + 1,
					MaxDepth))
				{
					return true; // Found a cycle
				}
			}
		}
	}
	
	// Remove from current path and mark as fully visited
	CurrentPath.Remove(CurrentEntity.EntityHandle);
	VisitedEntities.Add(CurrentEntity.EntityHandle);
	
	return false; // No cycle detected
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
	
	// maybe tryto use world condition from smart object ?
	return true;
}

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
} // namespace UE::Mass:Debug

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
		// Ideally we would have a way to register in the main picking flow but that would require more changes to
		// also support client-server picking. For now, we handle explicit mass picking requests on the authority
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
