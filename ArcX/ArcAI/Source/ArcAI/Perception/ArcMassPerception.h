// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassProcessor.h"

#include "NativeGameplayTags.h"

#include "ArcMassPerception.generated.h"

ARCAI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AI_Perception_Sense_Sight);
ARCAI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AI_Perception_Sense_Hearing);
ARCAI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AI_Perception_Stimuli_Sight);
ARCAI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AI_Perception_Stimuli_Hearing);

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float ForgetTime = 10.f;
	
    // Optional tag filter - only perceive entities with these tags
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGameplayTagContainer RequiredTags;

    // Optional tag filter - ignore entities with these tags  
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGameplayTagContainer IgnoredTags;
	
};

template<>
struct TMassFragmentTraits<FArcPerceptionSenseConfigFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
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
    float TimeSinceLastPerceived = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	double TimeFirstPerceived = 0.0f;
	
	UPROPERTY(BlueprintReadOnly)
	float TimePerceived = 0.0f;
	
	UPROPERTY(BlueprintReadOnly)
	double LastTimeSeen = 0.0f;
		
    bool operator==(const FArcPerceivedEntity& Other) const
    {
        return Entity == Other.Entity;
    }
	
	bool operator==(const FMassEntityHandle& Other) const
    {
    	return Entity == Other;
    }
	
	friend uint32 GetTypeHash(const FArcPerceivedEntity& Other)
	{
		return GetTypeHash(Other.Entity);
	}
};

// Fragment storing perceived entities per entity
USTRUCT()
struct ARCAI_API FArcMassPerceptionResultFragmentBase : public FMassFragment
{
    GENERATED_BODY()
	
public:
	UPROPERTY(VisibleAnywhere)
	TArray<FArcPerceivedEntity> PerceivedEntities;
	
	UPROPERTY(VisibleAnywhere)
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

// Native delegate types
DECLARE_MULTICAST_DELEGATE_ThreeParams(FArcPerceptionEntityAddedNative, FMassEntityHandle /*PerceiverEntity*/, FMassEntityHandle /*PerceivedEntity*/, FGameplayTag /*SenseTag*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FArcPerceptionEntityRemovedNative, FMassEntityHandle /*PerceiverEntity*/, FMassEntityHandle /*PerceivedEntity*/, FGameplayTag /*SenseTag*/);

DECLARE_MULTICAST_DELEGATE_ThreeParams(FArcPerceptionEntityList, FMassEntityHandle /*PerceiverEntity*/, const TArray<FArcPerceivedEntity>& /* PerceivedEntities */, FGameplayTag /*SenseTag*/);

namespace ArcPerception
{
	ARCAI_API bool PassesFilters(
		const FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const FArcPerceptionSenseConfigFragment& Config);
}

