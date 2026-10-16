/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#pragma once

#include "Engine/DataAsset.h"
#include "ArcNamedPrimaryAssetId.h"
#include "Items/ArcItemSpec.h"
#include "Mass/EntityHandle.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/StructView.h"
#include "WorldConditionQuery.h"

class UMassEntityConfigAsset;
class UMassEntitySubsystem;

// Forward-declare UArcLootTable so the namespace helper declaration below can reference it.
class UArcLootTable;

// Forward declaration for the schema-stamping helper (defined in ArcLootTable.cpp).
namespace ArcLootTableSchemaInternal
{
	ARCMASSITEMSRUNTIME_API void StampEntrySchemas(UArcLootTable& Table);
}

#include "ArcLootTable.generated.h"

/**
 * Base context struct passed to loot calculation. Derive to add custom fields
 * (player level, luck stat, etc.). Passed as FConstStructView.
 */
USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcLootContext
{
	GENERATED_BODY()

	// The entity that owns the loot table (loot source / self).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	FMassEntityHandle SelfEntity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	FMassEntityHandle InstigatorEntity;

	// Actor backing the self entity, if any.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	TWeakObjectPtr<AActor> SelfActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	TWeakObjectPtr<AActor> InstigatorActor;

	// Filled by UArcLootDropSubsystem before strategy runs — used by condition evaluation.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Loot")
	TWeakObjectPtr<class UMassEntitySubsystem> MassEntitySubsystem;

	// UObject outer required by FWorldConditionQueryState::Initialize. Subsystem sets to itself.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Loot")
	TWeakObjectPtr<const UObject> EvaluationOuter;

	virtual ~FArcLootContext() = default;
};

/** Result of a single loot drop calculation. */
USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcLootDropResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	FArcItemSpec ItemSpec;

	/** Entity config to use when spawning this drop. Carried from the entry. */
	UPROPERTY()
	TObjectPtr<const UMassEntityConfigAsset> LootEntityConfig = nullptr;
};

/**
 * Base loot table entry. Each entry rolls its own drop probability via GetDropProbability(),
 * then generates FMath::RandRange(MinAmount, MaxAmount) copies of the item.
 * Derive to add custom probability/weight logic.
 */
USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcLootTableEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (AllowedTypes = "ArcItem"))
	FArcNamedPrimaryAssetId ItemDefinitionId;

	/** Entity config asset for spawning this drop as a Mass entity. */
	UPROPERTY(EditAnywhere, Category = "Loot", meta = (DisplayThumbnail = false))
	TObjectPtr<UMassEntityConfigAsset> LootEntityConfig = nullptr;

	UPROPERTY(EditAnywhere, Category = "Loot", meta = (ClampMin = "1"))
	int32 MinAmount = 1;

	UPROPERTY(EditAnywhere, Category = "Loot", meta = (ClampMin = "1"))
	int32 MaxAmount = 1;

	/** Empty = always passes. Otherwise evaluated before probability roll. Designer picks a schema (default: UArcLootWorldConditionSchema). */
	UPROPERTY(EditAnywhere, Category = "Loot")
	FWorldConditionQueryDefinition Conditions;

	/** Returns the weight used for selection strategies. Base returns 1.0. */
	virtual float GetWeight() const { return 1.0f; }

	/** Returns the probability (0-1) that this entry drops at all. Base always drops. */
	virtual float GetDropProbability() const { return 1.0f; }

	/** Evaluates Conditions against the loot context. True when conditions are empty or pass. */
	virtual bool PassesConditions(FConstStructView Context) const;

	/** Calculate drops for this entry given context. Rolls probability gate, then appends to OutDrops. */
	virtual void CalculateDrops(FConstStructView Context, TArray<FArcLootDropResult>& OutDrops) const;

	virtual ~FArcLootTableEntry() = default;
};

/** Weighted entry with probability gate and selection weight. */
USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcLootTableEntry_Weighted : public FArcLootTableEntry
{
	GENERATED_BODY()

	/** Relative weight for weighted random selection. */
	UPROPERTY(EditAnywhere, Category = "Loot")
	float Weight = 1.0f;

	/** Probability (0-1) that this entry drops at all after being selected. */
	UPROPERTY(EditAnywhere, Category = "Loot", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DropProbability = 1.0f;

	virtual float GetWeight() const override { return Weight; }
	virtual float GetDropProbability() const override { return DropProbability; }
};

// ---------------------------------------------------------------------------
// Roll Strategy
// ---------------------------------------------------------------------------

/**
 * Base loot roll strategy. Iterates all entries and lets each roll independently.
 * Derive to implement custom selection algorithms (weighted random, pity, etc.).
 */
USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcLootRollStrategy
{
	GENERATED_BODY()

	/**
	 * Roll the loot table entries and produce drop results.
	 * @param Entries   The table's entry array (FInstancedStruct of FArcLootTableEntry).
	 * @param Context   Context for loot calculation.
	 * @param OutDrops  Results appended here.
	 */
	virtual void RollEntries(
		const TArray<FInstancedStruct>& Entries,
		FConstStructView Context,
		TArray<FArcLootDropResult>& OutDrops) const;

	virtual ~FArcLootRollStrategy() = default;
};

/**
 * Weighted random selection without replacement.
 * Entries failing their conditions are excluded before selection, then up to
 * MaxDrops entries are selected by weight and each selected entry rolls its
 * own CalculateDrops (which includes the probability gate).
 */
USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcLootRollStrategy_WeightedSelection : public FArcLootRollStrategy
{
	GENERATED_BODY()

	/** Maximum entries selected per roll. */
	UPROPERTY(EditAnywhere, Category = "Loot", meta = (ClampMin = "1"))
	int32 MaxDrops = 1;

	virtual void RollEntries(
		const TArray<FInstancedStruct>& Entries,
		FConstStructView Context,
		TArray<FArcLootDropResult>& OutDrops) const override;
};

// ---------------------------------------------------------------------------
// Loot Table Data Asset
// ---------------------------------------------------------------------------

/**
 * GUID-based primary data asset containing loot entries and a roll strategy.
 */
UCLASS(BlueprintType, meta = (LoadBehavior = "LazyOnDemand"))
class ARCMASSITEMSRUNTIME_API UArcLootTable : public UDataAsset
{
	GENERATED_BODY()

public:
	UArcLootTable();

	/** Loot entries (polymorphic, base struct = FArcLootTableEntry). */
	UPROPERTY(EditAnywhere, Category = "Loot", meta = (BaseStruct = "/Script/ArcMassItemsRuntime.ArcLootTableEntry", ExcludeBaseStruct))
	TArray<FInstancedStruct> Entries;

	/** Roll strategy determining how entries are evaluated. */
	UPROPERTY(EditAnywhere, Category = "Loot", meta = (BaseStruct = "/Script/ArcMassItemsRuntime.ArcLootRollStrategy", ExcludeBaseStruct))
	FInstancedStruct RollStrategy;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	virtual void PostLoad() override;
	virtual void PostInitProperties() override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

#if WITH_EDITORONLY_DATA
	/** Debug context for testing rolls in editor. */
	UPROPERTY(EditAnywhere, Category = "Debug", meta = (BaseStruct = "/Script/ArcMassItemsRuntime.ArcLootContext"))
	FInstancedStruct DebugContext;

	UPROPERTY(EditAnywhere, Category = "Debug", meta = (ClampMin = "1"))
	int32 DebugRollCount = 1000;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Debug")
	void DebugRollLootTable();
#endif

protected:
	UPROPERTY(VisibleAnywhere, Category = "Loot")
	FGuid LootTableId;

	UPROPERTY(EditAnywhere, Category = "Loot")
	FPrimaryAssetType LootTableType;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Loot")
	void RegenerateLootTableId();

	friend void ArcLootTableSchemaInternal::StampEntrySchemas(UArcLootTable& Table);
};
