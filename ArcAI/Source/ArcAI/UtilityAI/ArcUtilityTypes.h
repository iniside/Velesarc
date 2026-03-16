// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityHandle.h"
#include "StateTreeTypes.h"
#include "ArcUtilityTypes.generated.h"

struct FMassEntityManager;

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

	FVector GetLocation(const FMassEntityManager* EntityManager = nullptr) const;
	AActor* GetActor(const FMassEntityManager* EntityManager = nullptr) const;
};

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
