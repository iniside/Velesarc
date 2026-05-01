// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "MassEntityTypes.h"
#include "GameplayTagContainer.h"
#include "ArcEconomyTypes.h"
#include "ArcKnowledgeTypes.h"
#include "ArcMass/PlacedEntities/ArcEntityRef.h"
#include "Strategy/ArcStrategyTypes.h"
#include "ArcEconomyFragments.generated.h"

class UArcItemDefinition;
class UArcRecipeDefinition;
class UArcGovernorDataAsset;
class UArcStrategyActionSet;
class UMassEntityConfigAsset;
class UArcTQSQueryDefinition;

// ============================================================================
// Settlement Fragments
// ============================================================================

/** A building that has a specific item available in its output. */
USTRUCT()
struct ARCECONOMY_API FArcMarketSupplySource
{
    GENERATED_BODY()

    UPROPERTY()
    FMassEntityHandle BuildingHandle;

    UPROPERTY()
    int32 Quantity = 0;

    /** Quantity reserved by in-flight transport jobs. */
    UPROPERTY()
    int32 ReservedQuantity = 0;
};

/** Per-resource market data within a settlement. */
USTRUCT()
struct ARCECONOMY_API FArcResourceMarketData
{
    GENERATED_BODY()

    UPROPERTY()
    float Price = 0.0f;

    /** Items produced locally since last price tick. Reset each tick. */
    UPROPERTY()
    float SupplyCounter = 0.0f;

    /** Items consumed locally since last price tick. Reset each tick. */
    UPROPERTY()
    float DemandCounter = 0.0f;

    /** Buildings that currently have this item in output, with quantities.
     *  Maintained by ArcBuildingSupplyProcessor each tick. */
    UPROPERTY()
    TArray<FArcMarketSupplySource> SupplySources;
};

USTRUCT()
struct ARCECONOMY_API FArcSettlementFragment : public FMassFragment
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Settlement")
    FName SettlementName;

    /** True = player-owned, false = AI-governed. */
    UPROPERTY(EditAnywhere, Category = "Settlement")
    bool bPlayerOwned = false;

    UPROPERTY(EditAnywhere, Category = "Settlement")
    float SettlementRadius = 5000.0f;

    /** Designer-authored list of entities explicitly bound to this settlement by GUID. */
    UPROPERTY(EditAnywhere, Category = "Settlement")
    TArray<FArcEntityRef> LinkedEntities;

    /** How this settlement picks strategic actions (ML or Utility). */
    UPROPERTY(EditAnywhere, Category = "Strategy")
    EArcStrategyDecisionMode StrategyMode = EArcStrategyDecisionMode::Utility;

    /** Action set for utility scoring. Only used when StrategyMode == Utility. */
    UPROPERTY(EditAnywhere, Category = "Strategy")
    TObjectPtr<UArcStrategyActionSet> SettlementActionSet;

    /** MassEntityConfig defining the NPC archetype this settlement spawns. */
    UPROPERTY(EditAnywhere, Category = "Population")
    TObjectPtr<UMassEntityConfigAsset> NPCEntityConfig;

    /** TQS query for finding valid NPC spawn positions around this settlement.
      * Expected: circle/grid generator + nav projection step, TopN selection. */
    UPROPERTY(EditAnywhere, Category = "Population")
    TObjectPtr<UArcTQSQueryDefinition> SpawnLocationQuery;
};

USTRUCT()
struct ARCECONOMY_API FArcSettlementMarketFragment : public FMassFragment
{
    GENERATED_BODY()

    UPROPERTY()
    TMap<TObjectPtr<UArcItemDefinition>, FArcResourceMarketData> PriceTable;

    /** Price adjustment speed. Higher = more volatile. */
    UPROPERTY(EditAnywhere, Category = "Market")
    float K = 0.3f;

    /** Maximum total items storable across all buildings in the settlement. */
    UPROPERTY(EditAnywhere, Category = "Market")
    int32 TotalStorageCap = 500;

    /** Current total items stored across all buildings. Runtime tracked. */
    UPROPERTY()
    int32 CurrentTotalStorage = 0;
};

USTRUCT()
struct ARCECONOMY_API FArcSettlementWorkforceFragment : public FMassFragment
{
    GENERATED_BODY()

    UPROPERTY()
    int32 WorkerCount = 0;

    UPROPERTY()
    int32 TransporterCount = 0;

    UPROPERTY()
    int32 GathererCount = 0;

    UPROPERTY()
    int32 CaravanCount = 0;

    UPROPERTY()
    int32 IdleCount = 0;
};

USTRUCT()
struct ARCECONOMY_API FArcGovernorFragment : public FMassFragment
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Governor")
    TObjectPtr<UArcGovernorDataAsset> GovernorConfig = nullptr;

    /** World time when trade routes should next be evaluated. */
    double NextTradeEvalTime = 0.0;
};

// ============================================================================
// Building Fragments
// ============================================================================

/** An item a consumer building needs to keep stocked (e.g. tavern stocking food). */
USTRUCT()
struct ARCECONOMY_API FArcBuildingConsumptionEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    TObjectPtr<UArcItemDefinition> Item = nullptr;

    UPROPERTY(EditAnywhere)
    FGameplayTag ItemTag;

    UPROPERTY(EditAnywhere)
    int32 DesiredStockLevel = 5;
};

/** Per-production-slot workforce data. */
USTRUCT()
struct ARCECONOMY_API FArcBuildingSlotData
{
    GENERATED_BODY()

    /** Recipe this slot should produce. Persists when queue entry is removed. */
    UPROPERTY()
    TObjectPtr<UArcRecipeDefinition> DesiredRecipe = nullptr;

    /** How many workers this slot needs. */
    UPROPERTY()
    int32 RequiredWorkerCount = 1;

    /** Seconds since production was last halted (for demand priority). */
    UPROPERTY()
    float HaltDuration = 0.0f;

    /** One handle per recipe ingredient slot. Valid = that ingredient is missing and advertised. */
    UPROPERTY()
    TArray<FArcKnowledgeHandle> DemandKnowledgeHandles;
};

USTRUCT()
struct ARCECONOMY_API FArcBuildingFragment : public FMassFragment
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Building")
    FName BuildingName;

    UPROPERTY()
    FMassEntityHandle SettlementHandle;

    UPROPERTY()
    FVector BuildingLocation = FVector::ZeroVector;

    /** Current number of items sitting in this building's output buffer. Runtime tracked. */
    UPROPERTY()
    int32 CurrentOutputCount = 0;

    /** Knowledge handles for supply entries posted per output item type. Keyed by item definition. */
    UPROPERTY()
    TMap<TObjectPtr<UArcItemDefinition>, FArcKnowledgeHandle> SupplyKnowledgeHandles;

    /** One handle per ConsumptionNeeds entry. Indexed to match FArcBuildingEconomyConfig::ConsumptionNeeds. */
    UPROPERTY()
    TArray<FArcKnowledgeHandle> ConsumptionDemandHandles;

    /** Debug-only: when false, governor skips this building entirely. */
    UPROPERTY()
    bool bProductionEnabled = true;
};

USTRUCT()
struct ARCECONOMY_API FArcBuildingWorkforceFragment : public FMassFragment
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FArcBuildingSlotData> Slots;
};

USTRUCT()
struct ARCECONOMY_API FArcBuildingEconomyConfig : public FMassSharedFragment
{
    GENERATED_BODY()

    /** Explicit recipe list. If non-empty, used directly (manual mode — the default). */
    UPROPERTY(EditAnywhere, Category = "Economy")
    TArray<TObjectPtr<UArcRecipeDefinition>> AllowedRecipes;

    /**
     * Tags for dynamic recipe discovery via UArcRecipeDefinition::FilterByStationTags().
     * Used only when AllowedRecipes is empty.
     */
    UPROPERTY(EditAnywhere, Category = "Economy")
    FGameplayTagContainer ProductionStationTags;

    UPROPERTY(EditAnywhere, Category = "Economy")
    int32 WorkersPerSlot = 1;

    UPROPERTY(EditAnywhere, Category = "Economy")
    int32 MaxProductionSlots = 1;

    /** Activity tags that identify worker slots on the building's SmartObject.
     *  Used by the production gate to count only worker-staffed area slots. */
    UPROPERTY(EditAnywhere, Category = "Economy")
    FGameplayTagContainer WorkerActivityTags;

    /** Maximum number of items this building will hold in its output buffer before halting production. */
    UPROPERTY(EditAnywhere, Category = "Economy")
    int32 OutputBufferSize = 10;

    /** Items this building needs to keep stocked (consumer buildings like taverns). */
    UPROPERTY(EditAnywhere, Category = "Economy")
    TArray<FArcBuildingConsumptionEntry> ConsumptionNeeds;

    /** Planner-visible tags describing what this building requires from other plan entities. */
    UPROPERTY(EditAnywhere, Category = "Economy")
    FGameplayTagContainer RequiresTags;

    // --- Gathering config ---
    // If GatherOutputItems is non-empty, building is a gathering building.

    /** Items this building "produces" by gathering. Feeds demand graph as a leaf producer. */
    UPROPERTY(EditAnywhere, Category = "Gathering")
    TArray<TObjectPtr<UArcItemDefinition>> GatherOutputItems;

    /** Tags to match resource entities in the spatial hash. */
    UPROPERTY(EditAnywhere, Category = "Gathering")
    FGameplayTagContainer GatherSearchTags;

    /** Radius for spatial hash query from building location. */
    UPROPERTY(EditAnywhere, Category = "Gathering")
    float GatherSearchRadius = 5000.f;

    /** SmartObject activity tags for gatherer interaction slots on resource entities. */
    UPROPERTY(EditAnywhere, Category = "Gathering")
    FGameplayTagContainer GathererActivityTags;

    /** Resolved recipe list, populated on first access by ResolveAllowedRecipes. */

	UPROPERTY()
    TArray<TObjectPtr<UArcRecipeDefinition>> CachedRecipes;
    bool bRecipesCached = false;

    /** True if this building gathers resources instead of crafting. */
    bool IsGatheringBuilding() const { return GatherOutputItems.Num() > 0; }
};

// ============================================================================
// NPC Fragments
// ============================================================================

USTRUCT()
struct ARCECONOMY_API FArcEconomyNPCFragment : public FMassFragment
{
    GENERATED_BODY()

    UPROPERTY()
    FMassEntityHandle SettlementHandle;

    UPROPERTY()
    EArcEconomyNPCRole Role = EArcEconomyNPCRole::Idle;
};

USTRUCT()
struct ARCECONOMY_API FArcWorkerFragment : public FMassFragment
{
    GENERATED_BODY()

    UPROPERTY()
    FMassEntityHandle AssignedBuildingHandle;

    /** Set by the governor/backpressure system when the assigned building's output buffer is full. */
    UPROPERTY(VisibleAnywhere, Category = "Worker")
    bool bBackpressured = false;
};

USTRUCT()
struct ARCECONOMY_API FArcTransporterFragment : public FMassFragment
{
    GENERATED_BODY()

    UPROPERTY()
    EArcTransporterTaskState TaskState = EArcTransporterTaskState::Idle;

    UPROPERTY()
    FMassEntityHandle TargetBuildingHandle;

    UPROPERTY()
    TObjectPtr<UArcItemDefinition> TargetItemDefinition = nullptr;

    UPROPERTY()
    int32 TargetQuantity = 0;

    /** Building to pick up from. Set by transport manager. */
    UPROPERTY()
    FMassEntityHandle SourceBuildingHandle;

    /** Building to deliver to. Set by transport manager. */
    UPROPERTY()
    FMassEntityHandle DestinationBuildingHandle;
};

USTRUCT()
struct ARCECONOMY_API FArcGathererFragment : public FMassFragment
{
    GENERATED_BODY()

    UPROPERTY()
    FMassEntityHandle AssignedBuildingHandle;

    UPROPERTY()
    FMassEntityHandle TargetResourceHandle;

    UPROPERTY()
    bool bCarryingResource = false;
};

// ============================================================================
// Tags
// ============================================================================

USTRUCT()
struct ARCECONOMY_API FArcCaravanTag : public FMassTag
{
    GENERATED_BODY()
};

// ============================================================================
// Fragment Traits (opt-in for non-trivially-copyable fragments)
// ============================================================================

template <>
struct TMassFragmentTraits<FArcGovernorFragment> final
{
    enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

template <>
struct TMassFragmentTraits<FArcSettlementMarketFragment> final
{
    enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

template <>
struct TMassFragmentTraits<FArcBuildingFragment> final
{
    enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

template <>
struct TMassFragmentTraits<FArcBuildingWorkforceFragment> final
{
    enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

template <>
struct TMassFragmentTraits<FArcTransporterFragment> final
{
    enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

template <>
struct TMassFragmentTraits<FArcGathererFragment> final
{
    enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

template <>
struct TMassFragmentTraits<FArcBuildingEconomyConfig> final
{
    enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

template <>
struct TMassFragmentTraits<FArcSettlementFragment> final
{
    enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};
