// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassPerception.h"
#include "MassEntityFragments.h"
#include "MassProcessor.h"
#include "UObject/Object.h"
#include "ArcMassHearingPerception.generated.h"

class UArcMassSpatialHashSubsystem;
class UMassEntitySubsystem;

USTRUCT(BlueprintType)
struct ARCAI_API FArcPerceptionHearingSenseConfigFragment : public FArcPerceptionSenseConfigFragment
{
	GENERATED_BODY()

	// Maximum age (seconds) of a sound that can be heard. 0 = no age limit.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float SoundMaxAge = 0.5f;

	// Exponential decay rate for sound over time. 0 = linear-only (based on SoundMaxAge).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float SoundDecayRate = 2.0f;

	// Minimum strength required to register a sound.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float MinAudibleStrength = 0.05f;

	// Directional hearing multipliers (front/back).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float FrontHearingMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float BackHearingMultiplier = 0.6f;

	// Distance falloff exponent (1 = linear).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.1"))
	float DistanceFalloffExponent = 1.0f;

	// Approximate sound location settings.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bApproximateSoundLocation = true;

	// Expected max strength for normalization (used for error scaling).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float MaxHearingStrength = 1.0f;

	// Base positional error (units) added to approximated sound locations.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", EditCondition = "bApproximateSoundLocation"))
	float BaseLocationError = 50.0f;

	// Additional error per unit of distance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", EditCondition = "bApproximateSoundLocation"))
	float DistanceErrorScale = 0.1f;

	// Error scaling based on angle to forward (0 = no effect).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", EditCondition = "bApproximateSoundLocation"))
	float AngleErrorScale = 0.5f;

	// Error scaling based on weaker sounds (0 = no effect).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", EditCondition = "bApproximateSoundLocation"))
	float StrengthErrorScale = 1.0f;
};
template<>
struct TMassFragmentTraits<FArcPerceptionHearingSenseConfigFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

USTRUCT()
struct ARCAI_API FArcMassPerceivableStimuliHearingFragment : public FMassFragment
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

USTRUCT()
struct ARCAI_API FArcMassHearingPerceivableTag : public FMassTag
{
	GENERATED_BODY()
};


/**
 * Subsystem managing perception events and queries
 */
UCLASS()
class ARCAI_API UArcMassHearingPerceptionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	TMap<FMassEntityHandle, FArcPerceptionEntityAddedNative> OnEntityPerceived;
	TMap<FMassEntityHandle, FArcPerceptionEntityAddedNative> OnEntityLostFromPerception;
	
	TMap<FMassEntityHandle, FArcPerceptionEntityList> OnPerceptionUpdated;
	
	// Internal - called by processor to broadcast events
	void BroadcastEntityPerceived(FMassEntityHandle Perceiver, FMassEntityHandle Perceived, FGameplayTag SenseTag);
	void BroadcastEntityLostFromPerception(FMassEntityHandle Perceiver, FMassEntityHandle Perceived, FGameplayTag SenseTag);

private:
	TWeakObjectPtr<UMassEntitySubsystem> CachedEntitySubsystem;
	UMassEntitySubsystem* GetEntitySubsystem() const;
};

//----------------------------------------------------------------------
// Hearing Processor
//----------------------------------------------------------------------
UCLASS()
class ARCAI_API UArcMassHearingPerceptionProcessor : public UMassProcessor
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
		UArcMassHearingPerceptionSubsystem* PerceptionSubsystem,
		float DeltaTime,
		const TConstArrayView<FTransformFragment>& TransformList,
		const FArcPerceptionHearingSenseConfigFragment& Config,
		TArrayView<FArcMassPerceptionResultFragmentBase> ResultList);
	
	FMassEntityQuery HearingQuery;
	
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
