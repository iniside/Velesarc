// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "Mass/EntityHandle.h"
#include "Items/ArcItemSpec.h"
#include "Attributes/ArcAttribute.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcBuildPostPlaceAction.generated.h"

struct FMassEntityManager;
class UArcBuildingDefinition;
class UWorld;

/**
 * Everything a post-place action may need, captured once by the place command.
 * ConsumedItems are the FArcItemSpec snapshots reported by the resource consumption
 * step (Amount = stacks taken from that item); they outlive the store removal.
 */
struct FArcBuildPlacementContext
{
	const UArcBuildingDefinition* Def = nullptr;
	TConstArrayView<FArcItemSpec> ConsumedItems;
	FMassEntityHandle Instigator;
	FTransform Transform = FTransform::Identity;
	UWorld* World = nullptr;
};

/**
 * Base post-place action, configured per building definition (PostPlaceActions array).
 *
 * Two phases, both invoked by the server place command:
 * - BuildSpawnOverrides runs BEFORE the entity spawns. Sparse attribute fragments must
 *   land at Commit via SpawnEntityWithOverrides overrides (no post-spawn sparse-add mid
 *   processing), so any fragment the action wants on the entity is contributed here.
 * - PostPlaced runs AFTER SpawnEntityWithOverrides returns with the live entity handle.
 *
 * Ordering guard: UArcInitialAttributesObserver fires on FArcAggregatorFragment add
 * during Commit and re-seeds attribute fragments from its preset — it would stomp any
 * attribute values delivered purely as spawn overrides. Actions that write attributes
 * therefore RE-APPLY their computed values in PostPlaced (command context = game thread,
 * not mid-processing, so Commit and all creation observers have already run) — the
 * re-apply deterministically wins. BuildSpawnOverrides still runs first so the fragment
 * exists at Commit even on configs without an attributes trait.
 */
USTRUCT(BlueprintType)
struct ARCBUILDER_API FArcBuildPostPlaceAction
{
	GENERATED_BODY()

public:
	/** Contribute FInstancedStruct sparse-fragment overrides for SpawnEntityWithOverrides. */
	virtual void BuildSpawnOverrides(
		const FArcBuildPlacementContext& Ctx,
		TArray<FInstancedStruct>& InOutOverrides) const
	{
	}

	/** Runs after the placed entity exists (Commit + creation observers already ran). */
	virtual void PostPlaced(
		FMassEntityManager& EntityManager,
		FMassEntityHandle PlacedEntity,
		const FArcBuildPlacementContext& Ctx) const
	{
	}

	virtual ~FArcBuildPostPlaceAction() = default;
};

/** How per-ingredient stat values combine across all consumed items. */
UENUM(BlueprintType)
enum class EArcBuildStatAgg : uint8
{
	/** Sum of per-item values, weighted by stacks consumed. */
	Sum,
	/** Stack-weighted average of per-item values. */
	Average,
	/** Highest per-item value (stack count ignored). */
	Max,
};

/** One ingredient-stat -> placed-entity-attribute transfer rule. */
USTRUCT(BlueprintType)
struct ARCBUILDER_API FArcBuildStatMapping
{
	GENERATED_BODY()

public:
	/** Item stat (FArcItemFragment_ItemStats::DefaultStats key) read off each consumed item's definition. */
	UPROPERTY(EditAnywhere, Category = "Mapping")
	FArcAttributeRef SourceItemStat;

	/** Attribute on the placed entity to seed. FragmentType must derive from FArcMassAttributesBase. */
	UPROPERTY(EditAnywhere, Category = "Mapping")
	FArcAttributeRef TargetAttribute;

	UPROPERTY(EditAnywhere, Category = "Mapping")
	EArcBuildStatAgg Agg = EArcBuildStatAgg::Sum;

	/** Final multiplier applied to the aggregated value. */
	UPROPERTY(EditAnywhere, Category = "Mapping")
	float Scale = 1.f;
};

/**
 * Transfers stats authored on consumed ingredient items (FArcItemFragment_ItemStats,
 * ArcAbilityKit) onto attributes of the placed entity — e.g. wood Health -> wall MaxHealth.
 * Per consumed item, the matching DefaultStats entries are folded with the same
 * additive/multiplicative math as FArcItemFragment_ItemStats aggregation; per-item results
 * then combine across items via Agg and are scaled by Scale.
 * Values land both as spawn overrides (fragment present at Commit) and via a PostPlaced
 * re-apply (wins over UArcInitialAttributesObserver preset seeding — see base struct doc).
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Ingredient Stats Transfer"))
struct ARCBUILDER_API FArcBuildPostPlaceAction_IngredientStats : public FArcBuildPostPlaceAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Stats")
	TArray<FArcBuildStatMapping> Mappings;

	virtual void BuildSpawnOverrides(
		const FArcBuildPlacementContext& Ctx,
		TArray<FInstancedStruct>& InOutOverrides) const override;

	virtual void PostPlaced(
		FMassEntityManager& EntityManager,
		FMassEntityHandle PlacedEntity,
		const FArcBuildPlacementContext& Ctx) const override;
};
