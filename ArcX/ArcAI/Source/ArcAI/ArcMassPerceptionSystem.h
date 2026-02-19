// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "MassCommonTypes.h"
#include "MassEntityHandle.h"
#include "MassProcessor.h"
#include "ArcMass/ArcMassSpatialHashSubsystem.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ArcMassPerceptionSystem.generated.h"

USTRUCT(BlueprintType)
struct ARCAI_API FMassSightPerceptionFragment : public FMassFragment
{
    GENERATED_BODY()

    // Sight cone settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SightRange = 1500.0f; // 15 meters

    UPROPERTY(EditAnywhere, BlueprintReadWrite)  
    float SightAngleDegrees = 90.0f; // 90 degree cone

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ForgetTime = 5.0f; // Time to forget unseen targets

    // Perception update frequency (to avoid every-tick updates)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float UpdateInterval = 0.1f; // Update 10 times per second
    
    float LastUpdateTime = 0.0f;
};

template<>
struct TMassFragmentTraits<FMassSightPerceptionFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};


// Data about a perceived target
USTRUCT()
struct FMassPerceivedTarget
{
    GENERATED_BODY()

    // Entity that was perceived
    UPROPERTY()
    FMassEntityHandle TargetEntity;

    // Last known position of the target
    UPROPERTY()
    FVector LastKnownPosition = FVector::ZeroVector;

    // When this target was last seen
    UPROPERTY()
    float LastSeenTime = 0.0f;

    // How long ago target was seen
    UPROPERTY()
    float TimeSinceLastSeen = 0.0f;

    // Whether target is currently visible
    UPROPERTY()
    bool bCurrentlyVisible = false;

    // Strength of perception (1.0 = just seen, 0.0 = about to forget)
    UPROPERTY()
    float PerceptionStrength = 1.0f;

	// Calculate how likelyt are we to remember this entity. the longer it is in memory and not perceived the less likely we are to remember it and call it.
	float ChanceToRemember() const
	{
		return 1.f;
	}
	
    FMassPerceivedTarget() = default;

    FMassPerceivedTarget(FMassEntityHandle InTarget, const FVector& InPosition, float InTime)
        : TargetEntity(InTarget)
        , LastKnownPosition(InPosition)
        , LastSeenTime(InTime)
        , TimeSinceLastSeen(0.0f)
        , bCurrentlyVisible(true)
        , PerceptionStrength(1.0f)
    {}
};

// Fragment storing perceived targets
USTRUCT()
struct ARCAI_API FMassSightMemoryFragment : public FMassFragment
{
    GENERATED_BODY()

    // All currently remembered targets
    UPROPERTY()
    TArray<FMassPerceivedTarget> PerceivedTargets;

    // Add or update a perceived target
    void UpdateTarget(FMassEntityHandle TargetEntity, const FVector& Position, float CurrentTime)
    {
        // Find existing target
        FMassPerceivedTarget* ExistingTarget = PerceivedTargets.FindByPredicate(
            [TargetEntity](const FMassPerceivedTarget& Target)
            {
                return Target.TargetEntity == TargetEntity;
            });

        if (ExistingTarget)
        {
            // Update existing target
            ExistingTarget->LastKnownPosition = Position;
            ExistingTarget->LastSeenTime = CurrentTime;
            ExistingTarget->TimeSinceLastSeen = 0.0f;
            ExistingTarget->bCurrentlyVisible = true;
            ExistingTarget->PerceptionStrength = 1.0f;
        }
        else
        {
            // Add new target
            PerceivedTargets.Emplace(TargetEntity, Position, CurrentTime);
        }
    }

    // Get currently visible targets
    void GetVisibleTargets(TArray<FMassPerceivedTarget>& OutTargets) const
    {
        OutTargets.Reset();
        for (const FMassPerceivedTarget& Target : PerceivedTargets)
        {
            if (Target.bCurrentlyVisible)
            {
                OutTargets.Add(Target);
            }
        }
    }

    // Get all remembered targets (including fading ones)
    void GetAllRememberedTargets(TArray<FMassPerceivedTarget>& OutTargets) const
    {
        OutTargets = PerceivedTargets;
    }
};

template<>
struct TMassFragmentTraits<FMassSightMemoryFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};


// Tag for entities that can be perceived (targets)
USTRUCT()
struct ARCAI_API FMassPerceivableTag : public FMassTag
{
    GENERATED_BODY()
};

// Tag for entities that can perceive (observers)
USTRUCT()
struct ARCAI_API FMassPerceiverTag : public FMassTag
{
    GENERATED_BODY()
};

UCLASS()
class ARCAI_API UMassSightPerceptionProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassSightPerceptionProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	void ProcessNearbyEntitiesInBatches(
			FMassEntityManager& EntityManager,
			FMassExecutionContext& Context,
			const TArray<FArcMassEntityInfo>& NearbyEntities,
			const FVector& ObserverPos,
			const FVector& ObserverForward,
			const FMassSightPerceptionFragment& SightPerception,
			FMassSightMemoryFragment& SightMemory,
			float CurrentTime,
			UWorld* World);
private:
	// Query for entities that can perceive
	FMassEntityQuery PerceiverQuery;
    
	// Query for entities that can be perceived
	FMassEntityQuery PerceivableQuery;

	// Helper functions
	bool IsInSightCone(const FVector& ObserverPos, const FVector& ObserverForward, 
					  const FVector& TargetPos, float SightRange, float SightAngleRadians) const;
    
	bool HasLineOfSight(const FVector& Start, const FVector& End, UWorld* World) const;
    
	void UpdatePerceptualMemory(FMassSightMemoryFragment& Memory, float CurrentTime, float ForgetTime);
};

// Helper class for querying sight perception data
UCLASS(BlueprintType)
class ARCAI_API UMassSightPerceptionQueries : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Get all visible targets for an entity
	static bool GetVisibleTargets(UWorld* World, FMassEntityHandle Entity, TArray<FMassPerceivedTarget>& OutTargets);

	// Get all remembered targets (including fading ones) for an entity
	static bool GetRememberedTargets(UWorld* World, FMassEntityHandle Entity, TArray<FMassPerceivedTarget>& OutTargets);

	// Check if a specific target is currently visible to an entity
	static bool IsTargetVisible(UWorld* World, FMassEntityHandle Observer, FMassEntityHandle Target);

	// Get the closest visible target
	static bool GetClosestVisibleTarget(UWorld* World, FMassEntityHandle Observer, FMassPerceivedTarget& OutTarget);
};