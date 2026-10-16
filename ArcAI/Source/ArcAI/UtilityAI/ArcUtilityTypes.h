// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "StateTreeTypes.h"
#include "Abilities/ArcAbilityHandle.h"
#include "ArcUtilityTypes.generated.h"

struct FMassEntityManager;
class UArcAbilityDefinition;

UENUM(BlueprintType)
enum class EArcUtilityTargetType : uint8
{
	None,
	Actor,
	Entity,
	Location
};

UENUM(BlueprintType)
enum class EArcUtilitySelectionMode : uint8
{
	HighestScore,
	RandomFromTopPercent,
	WeightedRandom
};

UENUM(BlueprintType)
enum class EArcUtilityScoringStatus : uint8
{
	Pending,
	Processing,
	Selecting,
	Completed,
	Failed,
	Aborted
};

// Per-candidate axis: anything that varies inside an enumeration loop (one entry
// per candidate target, and for the ability pair, per candidate ability) belongs here.
USTRUCT(BlueprintType)
struct ARCAI_API FArcUtilityTarget
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EArcUtilityTargetType TargetType = EArcUtilityTargetType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TWeakObjectPtr<AActor> Actor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FMassEntityHandle EntityHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double TargetLastSeenTime = 0.0;

	// A position to act from, distinct from the target's own Location (e.g. cover point).
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasExecutionPosition = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector ExecutionPosition = FVector::ZeroVector;

	// The specific granted ability this candidate would use.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FArcAbilityHandle AbilityHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<const UArcAbilityDefinition> AbilityDefinition = nullptr;

	FVector GetLocation(const FMassEntityManager* EntityManager = nullptr) const;
	AActor* GetActor(const FMassEntityManager* EntityManager = nullptr) const;
};

// Per-request: one instance per FArcUtilityScoringInstance; the scoring loop
// mutates only EntryIndex. Anything that varies per candidate goes on FArcUtilityTarget instead.
USTRUCT(BlueprintType)
struct ARCAI_API FArcUtilityContext
{
	GENERATED_BODY()

	FMassEntityHandle QuerierEntity;
	FVector QuerierLocation = FVector::ZeroVector;
	FVector QuerierForward = FVector::ForwardVector;
	TWeakObjectPtr<AActor> QuerierActor;
	TWeakObjectPtr<UWorld> World;
	FMassEntityManager* EntityManager = nullptr;
	int32 EntryIndex = INDEX_NONE;
	bool bHasEligibleTarget = false;
};

USTRUCT(BlueprintType)
struct ARCAI_API FArcUtilityResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 WinningEntryIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	FArcUtilityTarget WinningTarget;

	UPROPERTY(BlueprintReadOnly)
	FStateTreeStateHandle WinningState;

	UPROPERTY(BlueprintReadOnly)
	float Score = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	bool bSuccess = false;
};
