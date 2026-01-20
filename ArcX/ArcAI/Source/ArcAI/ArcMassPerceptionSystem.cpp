// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassPerceptionSystem.h"

#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "ArcMass/ArcMassSpatialHashSubsystem.h"

UMassSightPerceptionProcessor::UMassSightPerceptionProcessor()
	: PerceiverQuery(*this)
	, PerceivableQuery(*this)
{
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    ExecutionFlags = (int32)EProcessorExecutionFlags::All;
}

void UMassSightPerceptionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    // Query for entities that can perceive others
    PerceiverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    PerceiverQuery.AddRequirement<FMassSightPerceptionFragment>(EMassFragmentAccess::ReadWrite);
    PerceiverQuery.AddRequirement<FMassSightMemoryFragment>(EMassFragmentAccess::ReadWrite);
    PerceiverQuery.AddTagRequirement<FMassPerceiverTag>(EMassFragmentPresence::All);
    PerceiverQuery.RegisterWithProcessor(*this);

    // Query for entities that can be perceived
    PerceivableQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    PerceivableQuery.AddRequirement<FArcMassSpatialHashFragment>(EMassFragmentAccess::ReadOnly);
    PerceivableQuery.AddTagRequirement<FMassPerceivableTag>(EMassFragmentPresence::All);
}

void UMassSightPerceptionProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = EntityManager.GetWorld();
    if (!World)
    {
        return;
    }

    UArcMassSpatialHashSubsystem* SpatialHashSubsystem = World->GetSubsystem<UArcMassSpatialHashSubsystem>();
    if (!SpatialHashSubsystem)
    {
        return;
    }

    const float CurrentTime = World->GetTimeSeconds();

	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
    // Process each perceiver

	TArray<FMassEntityHandle> EntitiesToSignal;
    PerceiverQuery.ForEachEntityChunk(Context,
        [this, SpatialHashSubsystem, CurrentTime, World, &EntityManager, &EntitiesToSignal](FMassExecutionContext& Ctx)
        {
            const TConstArrayView<FTransformFragment> TransformList = Ctx.GetFragmentView<FTransformFragment>();
            TArrayView<FMassSightPerceptionFragment> SightPerceptionList = Ctx.GetMutableFragmentView<FMassSightPerceptionFragment>();
            TArrayView<FMassSightMemoryFragment> SightMemoryList = Ctx.GetMutableFragmentView<FMassSightMemoryFragment>();

        	for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
        	{
                const FTransformFragment& Transform = TransformList[EntityIt];
                FMassSightPerceptionFragment& SightPerception = SightPerceptionList[EntityIt];
                FMassSightMemoryFragment& SightMemory = SightMemoryList[EntityIt];

                // Check if it's time to update this entity's perception
                if (CurrentTime - SightPerception.LastUpdateTime < SightPerception.UpdateInterval)
                {
                    // Still update memory decay
                    UpdatePerceptualMemory(SightMemory, CurrentTime, SightPerception.ForgetTime);
                    continue;
                }

                SightPerception.LastUpdateTime = CurrentTime;

                const FVector ObserverPos = Transform.GetTransform().GetLocation();
                const FVector ObserverForward = Transform.GetTransform().GetRotation().GetForwardVector();

                // Mark all current targets as not visible
                for (FMassPerceivedTarget& Target : SightMemory.PerceivedTargets)
                {
                    Target.bCurrentlyVisible = false;
                }

                // Query nearby entities using spatial hash
                TArray<FMassEntityHandle> NearbyEntityIndices = SpatialHashSubsystem->QueryEntitiesInRadius(
                    ObserverPos, SightPerception.SightRange);

                // Process nearby entities in batches using Mass collections
                if (!NearbyEntityIndices.IsEmpty())
                {
                    ProcessNearbyEntitiesInBatches(EntityManager, Ctx, NearbyEntityIndices,
                        ObserverPos, ObserverForward, SightPerception, SightMemory, CurrentTime, World);
                }

                // Update memory (handle forgetting)
                UpdatePerceptualMemory(SightMemory, CurrentTime, SightPerception.ForgetTime);

        		FMassEntityHandle Handle = Ctx.GetEntity(EntityIt);
            	EntitiesToSignal.Add(Handle);
            }
        });

	if (EntitiesToSignal.Num() > 0)
	{
		SignalSubsystem->SignalEntities("SightPerceptionChanged", EntitiesToSignal);
	}
}

void UMassSightPerceptionProcessor::ProcessNearbyEntitiesInBatches(
    FMassEntityManager& EntityManager,
    FMassExecutionContext& Context,
    const TArray<FMassEntityHandle>& NearbyEntities,
    const FVector& ObserverPos,
    const FVector& ObserverForward,
    const FMassSightPerceptionFragment& SightPerception,
    FMassSightMemoryFragment& SightMemory,
    float CurrentTime,
    UWorld* World)
{
    // Create entity collections for efficient batch processing
    TArray<FMassArchetypeEntityCollection> EntityCollections;
    UE::Mass::Utils::CreateEntityCollections(EntityManager, NearbyEntities, 
        FMassArchetypeEntityCollection::NoDuplicates, EntityCollections);

    const float SightAngleRadians = FMath::DegreesToRadians(SightPerception.SightAngleDegrees * 0.5f);

    // Process each collection - the query will automatically filter by FMassPerceivableTag
    PerceivableQuery.ForEachEntityChunkInCollections(EntityCollections, Context,
        [this, &ObserverPos, &ObserverForward, &SightPerception, &SightMemory, CurrentTime, World, SightAngleRadians]
						(FMassExecutionContext& Ctx)
        {
            const TConstArrayView<FTransformFragment> TargetTransforms = Ctx.GetFragmentView<FTransformFragment>();

            for (int32 TargetIndex = 0; TargetIndex < Ctx.GetNumEntities(); TargetIndex++)
            {
                const FMassEntityHandle TargetEntity = Ctx.GetEntity(TargetIndex);
                const FVector TargetPos = TargetTransforms[TargetIndex].GetTransform().GetLocation();

                // Check if target is in sight cone
                if (!IsInSightCone(ObserverPos, ObserverForward, TargetPos, 
                                 SightPerception.SightRange, SightAngleRadians))
                {
                    continue;
                }

                // Check line of sight (optional - can be expensive)
                //if (!HasLineOfSight(ObserverPos, TargetPos, World))
                //{
                //    continue;
                //}

                // Target is visible! Update memory
                SightMemory.UpdateTarget(TargetEntity, TargetPos, CurrentTime);
            }
        });
}

bool UMassSightPerceptionProcessor::IsInSightCone(const FVector& ObserverPos, const FVector& ObserverForward,
    const FVector& TargetPos, float SightRange, float SightAngleRadians) const
{
    const FVector ToTarget = TargetPos - ObserverPos;
    const float DistanceSquared = ToTarget.SizeSquared();

    // Check distance
    if (DistanceSquared > SightRange * SightRange)
    {
        return false;
    }

    // Check angle
    if (DistanceSquared > SMALL_NUMBER) // Avoid division by zero
    {
        const FVector ToTargetNormalized = ToTarget.GetSafeNormal();
        const float CosAngle = FVector::DotProduct(ObserverForward, ToTargetNormalized);
        const float RequiredCosAngle = FMath::Cos(SightAngleRadians);

        if (CosAngle < RequiredCosAngle)
        {
            return false;
        }
    }

    return true;
}

bool UMassSightPerceptionProcessor::HasLineOfSight(const FVector& Start, const FVector& End, UWorld* World) const
{
    // Simple line trace for line of sight
    // You might want to make this more sophisticated or optional for performance
    
    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.bTraceComplex = false;
    QueryParams.bReturnPhysicalMaterial = false;

    // Trace for blocking objects
    bool bHit = World->LineTraceSingleByChannel(
        HitResult,
        Start,
        End,
        ECC_Visibility, // Or your preferred collision channel
        QueryParams
    );

    // If we hit something, line of sight is blocked
    return !bHit;
}

void UMassSightPerceptionProcessor::UpdatePerceptualMemory(FMassSightMemoryFragment& Memory, 
    float CurrentTime, float ForgetTime)
{
    // Update all targets and remove forgotten ones
    for (int32 i = Memory.PerceivedTargets.Num() - 1; i >= 0; i--)
    {
        FMassPerceivedTarget& Target = Memory.PerceivedTargets[i];
        
        if (!Target.bCurrentlyVisible)
        {
            Target.TimeSinceLastSeen = CurrentTime - Target.LastSeenTime;
            
            // Calculate perception strength (1.0 = just seen, 0.0 = about to forget)
            Target.PerceptionStrength = FMath::Max(0.0f, 1.0f - (Target.TimeSinceLastSeen / ForgetTime));
            
            // Remove if completely forgotten
            if (Target.TimeSinceLastSeen >= ForgetTime)
            {
                Memory.PerceivedTargets.RemoveAtSwap(i);
            }
        }
        else
        {
            Target.TimeSinceLastSeen = 0.0f;
            Target.PerceptionStrength = 1.0f;
        }
    }
}

bool UMassSightPerceptionQueries::GetVisibleTargets(UWorld* World, FMassEntityHandle Entity, 
    TArray<FMassPerceivedTarget>& OutTargets)
{
    if (!World)
    {
        return false;
    }

    UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(World);
    if (!EntitySubsystem)
    {
        return false;
    }
	const FMassEntityManager& EM  = EntitySubsystem->GetEntityManager(); 
    FMassSightMemoryFragment* SightMemory = EM.GetFragmentDataPtr<FMassSightMemoryFragment>(Entity);
    if (!SightMemory)
    {
        return false;
    }

    SightMemory->GetVisibleTargets(OutTargets);
    return true;
}

bool UMassSightPerceptionQueries::GetRememberedTargets(UWorld* World, FMassEntityHandle Entity, 
    TArray<FMassPerceivedTarget>& OutTargets)
{
    if (!World)
    {
        return false;
    }

    UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(World);
    if (!EntitySubsystem)
    {
        return false;
    }

	const FMassEntityManager& EM  = EntitySubsystem->GetEntityManager();
    FMassSightMemoryFragment* SightMemory = EM.GetFragmentDataPtr<FMassSightMemoryFragment>(Entity);
    if (!SightMemory)
    {
        return false;
    }

    SightMemory->GetAllRememberedTargets(OutTargets);
    return true;
}

bool UMassSightPerceptionQueries::IsTargetVisible(UWorld* World, FMassEntityHandle Observer, FMassEntityHandle Target)
{
    TArray<FMassPerceivedTarget> VisibleTargets;
    if (!GetVisibleTargets(World, Observer, VisibleTargets))
    {
        return false;
    }

    return VisibleTargets.ContainsByPredicate([Target](const FMassPerceivedTarget& PerceivedTarget)
    {
        return PerceivedTarget.TargetEntity == Target;
    });
}

bool UMassSightPerceptionQueries::GetClosestVisibleTarget(UWorld* World, FMassEntityHandle Observer, 
    FMassPerceivedTarget& OutTarget)
{
    if (!World)
    {
        return false;
    }

    UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(World);
    if (!EntitySubsystem)
    {
        return false;
    }

	const FMassEntityManager& EM  = EntitySubsystem->GetEntityManager();
    // Get observer position
    const FTransformFragment* ObserverTransform = EM.GetFragmentDataPtr<FTransformFragment>(Observer);
    if (!ObserverTransform)
    {
        return false;
    }

    TArray<FMassPerceivedTarget> VisibleTargets;
    if (!GetVisibleTargets(World, Observer, VisibleTargets))
    {
        return false;
    }

    if (VisibleTargets.IsEmpty())
    {
        return false;
    }

    const FVector ObserverPos = ObserverTransform->GetTransform().GetLocation();
    float ClosestDistSq = FLT_MAX;
    int32 ClosestIndex = 0;

    for (int32 i = 0; i < VisibleTargets.Num(); i++)
    {
        float DistSq = FVector::DistSquared(ObserverPos, VisibleTargets[i].LastKnownPosition);
        if (DistSq < ClosestDistSq)
        {
            ClosestDistSq = DistSq;
            ClosestIndex = i;
        }
    }

    OutTarget = VisibleTargets[ClosestIndex];
    return true;
}