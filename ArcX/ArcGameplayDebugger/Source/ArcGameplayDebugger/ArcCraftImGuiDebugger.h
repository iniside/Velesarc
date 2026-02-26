// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "ArcCraft/MaterialCraft/ArcMaterialCraftContext.h"
#include "ArcCraft/MaterialCraft/ArcMaterialCraftEvaluator.h"
#include "ArcCraft/Recipe/ArcCraftSlotResolver.h"

class UArcCraftStationComponent;
class UArcRecipeDefinition;
class UArcMaterialPropertyTable;
class UArcItemsStoreComponent;
class UArcItemDefinition;
struct FArcItemData;
struct FArcMaterialQualityBand;

/** Result of Monte Carlo simulation for a single rule+band combination. */
struct FArcCraftImGuiSimBandResult
{
	FString RuleName;
	FString BandName;
	int32 RuleIndex = INDEX_NONE;
	int32 BandIndex = INDEX_NONE;
	int32 HitCount = 0;
	FString ModifierSummary;
	TMap<FString, float> AccumulatedStatValues;
};

/**
 * ImGui debug window for inspecting and testing the ArcCraft crafting system.
 * Mirrors the functionality of FArcCraftDebugWindow but uses direct ImGui calls.
 *
 * Features:
 * - Browse all UArcCraftStationComponent in the world
 * - Queue recipes with optional ingredient bypass and instant crafting
 * - Transfer items from player inventory to station
 * - Browse all recipe definitions with detail view
 * - Material property preview: simulate the evaluation pipeline,
 *   inspect rule matching, band probabilities, and output modifiers
 */
class FArcCraftImGuiDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	mutable TWeakObjectPtr<UWorld> World;

	// ---- Tab: Stations ----

	void DrawStationsTab();
	void DrawStationList();
	void DrawStationDetail();
	void DrawCraftQueue(UArcCraftStationComponent* Station);
	void DrawAvailableRecipes(UArcCraftStationComponent* Station);
	void DrawTransferItemsPanel(UArcCraftStationComponent* Station);
	void DrawCraftableDiscovery(UArcCraftStationComponent* Station);

	TArray<TWeakObjectPtr<UArcCraftStationComponent>> CachedStations;
	int32 SelectedStationIndex = -1;

	bool bBypassIngredients = false;
	bool bInstantCraft = false;

	TArray<UArcRecipeDefinition*> CachedStationRecipes;
	bool bStationRecipesCached = false;

	bool bShowTransferPanel = false;
	TArray<FAssetData> TransferItemDefinitions;
	TArray<FString> TransferItemNames;
	int32 SelectedTransferItemIndex = 0;
	char TransferFilterBuf[256] = {};

	// ---- Tab: Recipes ----

	void DrawRecipesTab();
	void DrawRecipeList();
	void DrawRecipeDetail(UArcRecipeDefinition* Recipe);

	TArray<FAssetData> CachedRecipeDefinitions;
	TArray<FString> CachedRecipeNames;
	int32 SelectedRecipeIndex = -1;
	char RecipeFilterBuf[256] = {};

	// ---- Tab: Material Preview ----

	void DrawMaterialPreviewTab();
	void DrawMaterialRecipeSelector();
	void DrawSimulatedIngredients();
	void DrawMaterialContextDisplay(const FArcMaterialCraftContext& Context);
	void DrawRuleEvaluations(const UArcMaterialPropertyTable* Table, const FArcMaterialCraftContext& Context);
	void DrawBandProbabilities(const FArcMaterialPropertyRule& Rule, float BandEligQ, float WeightBonus);
	void DrawBandModifierDetails(const FArcMaterialQualityBand& Band, float BandEligQ);
	void DrawOutputPreview();
	void DrawPendingModifiersTable();
	void DrawSimulationPanel(const UArcMaterialPropertyTable* Table, const FArcMaterialCraftContext& Context);
	void RunMonteCarloSimulation(const UArcMaterialPropertyTable* Table, const FArcMaterialCraftContext& Context);

	TArray<FAssetData> CachedMaterialRecipes;
	TArray<FString> CachedMaterialRecipeNames;
	int32 SelectedMaterialRecipeIndex = -1;
	char MaterialRecipeFilterBuf[256] = {};
	bool bMaterialRecipesCached = false;

	float QualityOverride = 1.0f;
	bool bUseQualityOverride = false;

	char PreviewTagsBuf[512] = {};
	FGameplayTagContainer PreviewTags;
	bool bUsePreviewTags = false;

	FArcMaterialCraftContext CachedMaterialContext;
	TArray<FArcMaterialRuleEvaluation> CachedEvaluations;
	TArray<FArcCraftPendingModifier> CachedPendingModifiers;
	bool bEvaluationDirty = true;

	int32 SimulationIterations = 500;
	TArray<FArcCraftImGuiSimBandResult> SimulationResults;
	bool bSimulationDirty = true;

	// ---- Helpers ----

	void RefreshStations();
	void RefreshRecipeDefinitions();
	void RefreshTransferItemDefinitions();
	void RefreshMaterialRecipes();
	UArcItemsStoreComponent* GetLocalPlayerItemStore() const;
	APlayerController* GetLocalPlayerController() const;

	static float ComputeBandWeight(
		float BaseWeight, float MinQuality, float QualityWeightBias,
		float BandEligibilityQuality, float BandWeightBonus);

	/** Helper: filter a list of names by a search buffer, returns filtered indices into original array. */
	static void FilterNames(const TArray<FString>& Source, const char* FilterBuf, TArray<int32>& OutFilteredIndices);

	/** Current tab index for the main tab bar. */
	int32 CurrentTab = 0;
};
