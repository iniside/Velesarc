// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArcAreaTypes.h"
#include "MassEntityTypes.h"
#include "ArcAreaPopulationSpawner.generated.h"

class UArcAreaPopulationDefinition;
class UArcAreaSubsystem;
class UArcTQSQueryDefinition;
class UBillboardComponent;

/**
 * Monitors area vacancies and spawns NPC Mass entities to fill them.
 *
 * Place ONE per town/level with a UArcAreaPopulationDefinition.
 * On BeginPlay, listens to UArcAreaSubsystem::OnSlotStateChanged globally.
 * When a slot becomes Vacant whose role matches an entry in the definition,
 * spawns a Mass entity from the matching config at the area's location.
 *
 * Tracks spawned count per role to respect MaxCount limits.
 * Note: MaxCount is a ceiling on total spawns, not active count.
 */
UCLASS(hidecategories = (Object, Input, Rendering, LOD, Cooking, Collision, HLOD, Partition))
class ARCAREA_API AArcAreaPopulationSpawner : public AActor
{
	GENERATED_BODY()

public:
	AArcAreaPopulationSpawner();

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> SpriteComponent;
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void OnSlotStateChanged(const FArcAreaSlotHandle& SlotHandle, EArcAreaSlotState NewState);

	void SpawnForSlot(const FArcAreaSlotHandle& SlotHandle);
	void ExecuteSpawn(const FArcAreaSlotHandle& SlotHandle, const FGameplayTag& RoleTag, const FVector& SpawnLocation);

	/** Population definition asset. */
	UPROPERTY(EditAnywhere, Category = "Population")
	TObjectPtr<UArcAreaPopulationDefinition> PopulationDefinition;

	/**
	 * Optional TQS query to find valid spawn locations near each area.
	 * The query runs with the area's world position as the querier location.
	 * Configure with a Grid/Circle generator + NavProjection step for best results.
	 * If not set, entities spawn at the area's exact location.
	 */
	UPROPERTY(EditAnywhere, Category = "Population")
	TObjectPtr<UArcTQSQueryDefinition> SpawnLocationQuery;

	/** Delay before initial vacancy scan after BeginPlay (seconds). Allows areas to register first. */
	UPROPERTY(EditAnywhere, Category = "Population", meta = (ClampMin = 0.0))
	float InitialScanDelay = 0.5f;

	FDelegateHandle SlotStateChangedHandle;
	FTimerHandle InitialScanTimerHandle;

	/** Current spawned count per role tag. */
	TMap<FGameplayTag, int32> SpawnedCounts;

	/** IDs of in-flight TQS queries (for cleanup on EndPlay). */
	TArray<int32> ActiveQueryIds;
};
