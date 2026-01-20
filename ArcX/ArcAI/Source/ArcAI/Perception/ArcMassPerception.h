// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MassEntityFragments.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/Object.h"
#include "MassEntityTypes.h"
#include "MassProcessor.h"
#include "MassObserverProcessor.h"

#include "NativeGameplayTags.h"

#include "ArcMassPerception.generated.h"

class UMassEntitySubsystem;
/**
 * 
 */
UCLASS()
class ARCAI_API UArcMassPerception : public UObject
{
	GENERATED_BODY()
};

#pragma once


class UArcMassSpatialHashSubsystem;

// Perception shape type
UENUM(BlueprintType)
enum class EArcPerceptionShapeType : uint8
{
    Radius,
    Cone
};

// Configuration for a perception sense
USTRUCT(BlueprintType)
struct ARCAI_API FArcPerceptionSenseConfigFragment : public FMassConstSharedFragment
{
    GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sense")
	FGameplayTag SenseTag;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sense")
	FGameplayTagContainer RequiredStimuliTags;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float EyeOffset = 100.0f;
	
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EArcPerceptionShapeType ShapeType = EArcPerceptionShapeType::Radius;

    // For radius-based perception
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "ShapeType == EArcPerceptionShapeType::Radius"))
    float Radius = 1000.0f;

    // For cone-based perception
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "ShapeType == EArcPerceptionShapeType::Cone"))
    float ConeLength = 1500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "ShapeType == EArcPerceptionShapeType::Cone", ClampMin = "1.0", ClampMax = "180.0"))
    float ConeHalfAngleDegrees = 45.0f;

    // How often to update perception (0 = every frame)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
    float UpdateInterval = 0.1f;

    // Optional tag filter - only perceive entities with these tags
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGameplayTagContainer RequiredTags;

    // Optional tag filter - ignore entities with these tags  
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGameplayTagContainer IgnoredTags;
	
	// Debug drawing
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bEnableDebugDraw = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (EditCondition = "bEnableDebugDraw"))
	FColor DebugShapeColor = FColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (EditCondition = "bEnableDebugDraw"))
	FColor DebugPerceivedLineColor = FColor::Red;
};

// Single perceived entity data
USTRUCT(BlueprintType)
struct ARCAI_API FArcPerceivedEntity
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FMassEntityHandle Entity;

    UPROPERTY(BlueprintReadOnly)
    FVector LastKnownLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    float Distance = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float Strength = 0.0f;
	
    UPROPERTY(BlueprintReadOnly)
    float TimeSinceLastSeen = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	FGameplayTag DetectedBySense;
	
    bool operator==(const FArcPerceivedEntity& Other) const
    {
        return Entity == Other.Entity;
    }
};

// Native delegate types
DECLARE_MULTICAST_DELEGATE_ThreeParams(FArcPerceptionEntityAddedNative, FMassEntityHandle /*PerceiverEntity*/, FMassEntityHandle /*PerceivedEntity*/, FGameplayTag /*SenseTag*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FArcPerceptionEntityRemovedNative, FMassEntityHandle /*PerceiverEntity*/, FMassEntityHandle /*PerceivedEntity*/, FGameplayTag /*SenseTag*/);

USTRUCT(BlueprintType)
struct ARCAI_API FArcPerceptionSenseResult
{
	GENERATED_BODY()

	// The sense this result belongs to
	UPROPERTY(BlueprintReadOnly)
	FGameplayTag SenseTag;

	// Currently perceived entities by this sense
	UPROPERTY(BlueprintReadOnly)
	TArray<FArcPerceivedEntity> PerceivedEntities;

	// Time accumulator for update interval
	float TimeSinceLastUpdate = 0.0f;
};


// Fragment storing perceived entities per entity
USTRUCT()
struct ARCAI_API FArcMassPerceptionResultFragmentBase : public FMassFragment
{
    GENERATED_BODY()
	
public:
    //UPROPERTY(EditAnywhere)
    //FArcPerceptionSenseConfig Config;
    
	TArray<FArcPerceivedEntity> PerceivedEntities;
	float TimeSinceLastUpdate = 0.0f;

	void RemoveEntity(FMassEntityHandle EntityHandle)
	{
		PerceivedEntities.RemoveAll([EntityHandle](const FArcPerceivedEntity& PE) { return PE.Entity == EntityHandle; });
	}
	
	TArray<FMassEntityHandle> GetEntities() const
	{
		TArray<FMassEntityHandle> Result;
		Algo::Transform(PerceivedEntities, Result, [](const FArcPerceivedEntity& PE) { return PE.Entity; });
		
		return Result;
	}
	
	bool IsEntityPerceived(FMassEntityHandle Entity) const
	{
		return PerceivedEntities.ContainsByPredicate([Entity](const FArcPerceivedEntity& PE)
		{
			return PE.Entity == Entity;
		});
	}

	bool GetClosestEntity(FMassEntityHandle& OutEntity, float& OutDistance) const
	{
		if (PerceivedEntities.IsEmpty())
		{
			return false;
		}

		const FArcPerceivedEntity* Closest = &PerceivedEntities[0];
		for (const FArcPerceivedEntity& PE : PerceivedEntities)
		{
			if (PE.Distance < Closest->Distance)
			{
				Closest = &PE;
			}
		}

		OutEntity = Closest->Entity;
		OutDistance = Closest->Distance;
		return true;
	}

	int32 GetPerceivedCount() const
	{
		return PerceivedEntities.Num();
	}
};

template<>
struct TMassFragmentTraits<FArcMassPerceptionResultFragmentBase> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

//----------------------------------------------------------------------
// Concrete sense fragments - add one for each sense type you need
//----------------------------------------------------------------------

// Sight perception result
USTRUCT()
struct ARCAI_API FArcMassSightPerceptionResult : public FArcMassPerceptionResultFragmentBase
{
	GENERATED_BODY()
};

template<>
struct TMassFragmentTraits<FArcMassSightPerceptionResult> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

// Hearing perception result
USTRUCT()
struct ARCAI_API FArcMassHearingPerceptionResult : public FArcMassPerceptionResultFragmentBase
{
	GENERATED_BODY()
	
};

template<>
struct TMassFragmentTraits<FArcMassHearingPerceptionResult> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

// Generic/custom perception result (for user-defined senses)
USTRUCT()
struct ARCAI_API FArcMassGenericPerceptionResult : public FArcMassPerceptionResultFragmentBase
{
	GENERATED_BODY()
};

template<>
struct TMassFragmentTraits<FArcMassGenericPerceptionResult> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

// Tag to mark entities as perceivable (they will be found by perception queries)
USTRUCT()
struct ARCAI_API FArcMassPerceivableTag : public FMassTag
{
    GENERATED_BODY()
};

// Fragment for perceivable entities - defines what stimuli they emit
USTRUCT()
struct ARCAI_API FArcMassPerceivableStimuliFragment : public FMassFragment
{
	GENERATED_BODY()

	// What stimuli this entity emits (e.g., "Perception.Stimuli.Visual", "Perception.Stimuli.Audio")
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer StimuliTags;
};

template<>
struct TMassFragmentTraits<FArcMassPerceivableStimuliFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

USTRUCT()
struct ARCAI_API FArcMassPerceivableStimuliSightFragment : public FArcMassPerceivableStimuliFragment
{
	GENERATED_BODY()
};

template<>
struct TMassFragmentTraits<FArcMassPerceivableStimuliSightFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

USTRUCT()
struct ARCAI_API FArcMassPerceivableStimuliHearingFragment : public FArcMassPerceivableStimuliFragment
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere)
	bool bEmittedSound = false;
	
	UPROPERTY(EditAnywhere)
	float EmissionStrength = 0;
	
	UPROPERTY(EditAnywhere)
	double EmissionTime = 0;
};

template<>
struct TMassFragmentTraits<FArcMassPerceivableStimuliHearingFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

/**
 * Subsystem managing perception events and queries
 */
UCLASS()
class ARCAI_API UArcMassPerceptionSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
	
	TMap<FMassEntityHandle, FArcPerceptionEntityAddedNative> OnEntityPerceived;
	TMap<FMassEntityHandle, FArcPerceptionEntityAddedNative> OnEntityLostFromPerception;
	
    // Internal - called by processor to broadcast events
    void BroadcastEntityPerceived(FMassEntityHandle Perceiver, FMassEntityHandle Perceived, FGameplayTag SenseTag);
    void BroadcastEntityLostFromPerception(FMassEntityHandle Perceiver, FMassEntityHandle Perceived, FGameplayTag SenseTag);

private:
    TWeakObjectPtr<UMassEntitySubsystem> CachedEntitySubsystem;
    UMassEntitySubsystem* GetEntitySubsystem() const;
};

/**
 * Processor that updates perception for all entities with perception components
 */
UCLASS(Abstract)
class ARCAI_API UArcMassPerceptionProcessorBase : public UMassProcessor
{
    GENERATED_BODY()

public:
    UArcMassPerceptionProcessorBase();

protected:
    // Only processor-level config: which sense tag this processor handles
    UPROPERTY(EditDefaultsOnly, Category = "Perception")
    FGameplayTag SenseTag;

    FMassEntityQuery PerceptionQuery;

    static bool PassesFilters(
        const FMassEntityManager& EntityManager,
        FMassEntityHandle Entity,
        const FArcPerceptionSenseConfigFragment& Config);

#if WITH_GAMEPLAY_DEBUGGER
    static void DrawDebugPerception(
        UWorld* World,
        const FVector& Location,
        const FQuat& Rotation,
        const FArcPerceptionSenseConfigFragment& Config,
        const FArcMassPerceptionResultFragmentBase& Result);
#endif
};

//----------------------------------------------------------------------
// Sight Processor
//----------------------------------------------------------------------
UCLASS()
class ARCAI_API UArcMassSightPerceptionProcessor : public UArcMassPerceptionProcessorBase
{
    GENERATED_BODY()

public:
    UArcMassSightPerceptionProcessor();

protected:
    virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	
	// Core processing - reads config from each entity's fragment
	void ProcessPerceptionChunk(
		FMassEntityManager& EntityManager,
		FMassExecutionContext& Context,
		UArcMassSpatialHashSubsystem* SpatialHash,
		UArcMassPerceptionSubsystem* PerceptionSubsystem,
		float DeltaTime,
		const TConstArrayView<FTransformFragment>& TransformList,
		const FArcPerceptionSenseConfigFragment& Config,
		TArrayView<FArcMassPerceptionResultFragmentBase> ResultList);
};

//----------------------------------------------------------------------
// Hearing Processor
//----------------------------------------------------------------------
UCLASS()
class ARCAI_API UArcMassHearingPerceptionProcessor : public UArcMassPerceptionProcessorBase
{
    GENERATED_BODY()

public:
    UArcMassHearingPerceptionProcessor();

protected:
    virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	
	// Core processing - reads config from each entity's fragment
	void ProcessPerceptionChunk(
		FMassEntityManager& EntityManager,
		FMassExecutionContext& Context,
		UArcMassSpatialHashSubsystem* SpatialHash,
		UArcMassPerceptionSubsystem* PerceptionSubsystem,
		float DeltaTime,
		const TConstArrayView<FTransformFragment>& TransformList,
		const FArcPerceptionSenseConfigFragment& Config,
		TArrayView<FArcMassPerceptionResultFragmentBase> ResultList);
	
	FMassEntityQuery HearingQuery;
};

//----------------------------------------------------------------------
// Observer processor for cleanup
//----------------------------------------------------------------------
UCLASS()
class ARCAI_API UArcMassPerceptionObserver : public UMassObserverProcessor
{
    GENERATED_BODY()

public:
    UArcMassPerceptionObserver();

protected:
    virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
    FMassEntityQuery ObserverQuery;
};