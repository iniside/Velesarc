// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityHandle.h"
#include "MassEntityTypes.h"
#include "SmartObjectTypes.h"
#include "ArcTQSTypes.generated.h"

UENUM(BlueprintType)
enum class EArcTQSTargetType : uint8
{
	None,
	MassEntity,
	Actor,
	Location,
	Object,
	SmartObject
};

UENUM(BlueprintType)
enum class EArcTQSQueryStatus : uint8
{
	Pending,
	Generating,
	Processing,
	Selecting,
	Completed,
	Failed,
	Aborted
};

UENUM(BlueprintType)
enum class EArcTQSSelectionMode : uint8
{
	HighestScore,
	TopN,
	AllPassing,
	RandomWeighted
};

USTRUCT(BlueprintType)
struct ARCAI_API FArcTQSTargetItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EArcTQSTargetType TargetType = EArcTQSTargetType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FMassEntityHandle EntityHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TWeakObjectPtr<AActor> Actor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TWeakObjectPtr<UObject> Object;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Location = FVector::ZeroVector;

	// Smart object handles (valid when TargetType == SmartObject)
	FSmartObjectHandle SmartObjectHandle;
	FSmartObjectSlotHandle SlotHandle;

	// Running score — starts at 1.0 for multiplicative model
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Score = 1.0f;

	// Whether this item passed all filters so far
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bValid = true;

	// Index into QueryContext.ContextLocations identifying which context generated this item.
	// Set automatically by the generator. INDEX_NONE if not associated with a specific context.
	int32 ContextIndex = INDEX_NONE;

	// Resolve the world location regardless of target type
	FVector GetLocation(const FMassEntityManager* EntityManager = nullptr) const;

	// Resolve the actor (from entity if needed)
	AActor* GetActor(const FMassEntityManager* EntityManager = nullptr) const;

	// Resolve entity handle (from actor if needed)
	FMassEntityHandle GetEntityHandle(const FMassEntityManager* EntityManager = nullptr) const;
};

USTRUCT(BlueprintType)
struct ARCAI_API FArcTQSQueryContext
{
	GENERATED_BODY()

	// The entity that initiated this query
	FMassEntityHandle QuerierEntity;

	// Querier's location (cached at query start)
	FVector QuerierLocation = FVector::ZeroVector;

	// Querier's forward direction
	FVector QuerierForward = FVector::ForwardVector;

	// The world
	TWeakObjectPtr<UWorld> World;

	// Entity manager reference
	FMassEntityManager* EntityManager = nullptr;

	// For ExplicitList generator: items injected from State Tree
	TArray<FArcTQSTargetItem> ExplicitItems;

	// Optional querier actor (if entity has actor fragment)
	TWeakObjectPtr<AActor> QuerierActor;

	/**
	 * Context locations that generators run around.
	 * For example: smart object locations, patrol waypoints, or any array of points.
	 * Generators produce items around EACH context location.
	 *
	 * If empty when the query starts, QuerierLocation is automatically added as the sole context location.
	 * If populated, QuerierLocation is NOT automatically added — add it explicitly if desired.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVector> ContextLocations;
};
