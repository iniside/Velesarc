// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "SlateIMWidgetBase.h"
#include "AssetRegistry/AssetData.h"
#include "ArcCraft/MaterialCraft/ArcMaterialCraftContext.h"
#include "ArcCraft/MaterialCraft/ArcMaterialCraftEvaluator.h"

class UArcCraftStationComponent;
class UArcRecipeDefinition;
class UArcMaterialPropertyTable;
class UArcItemsStoreComponent;
class UArcItemDefinition;
struct FArcItemData;

/**
 * SlateIM debug window for inspecting and testing the ArcCraft crafting system.
 *
 * Features:
 * - Browse all UArcCraftStationComponent in the world
 * - Queue recipes with optional ingredient bypass and instant crafting
 * - Transfer items from player inventory to station
 * - Browse all recipe definitions with detail view
 * - Material property preview: simulate the evaluation pipeline,
 *   inspect rule matching, band probabilities, and output modifiers
 *
 * Console command: Arc.Craft.Debug
 */
class FArcCraftDebugWindow : public FSlateIMWindowBase
{
public:
	FArcCraftDebugWindow();

	mutable TWeakObjectPtr<UWorld> World;

protected:
	virtual void DrawWindow(float DeltaTime) override;

private:
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

	// Debug overrides
	bool bBypassIngredients = false;
	bool bInstantCraft = false;

	// Station recipe list
	TArray<UArcRecipeDefinition*> CachedStationRecipes;
	bool bStationRecipesCached = false;

	// Transfer items
	bool bShowTransferPanel = false;
	TArray<FAssetData> TransferItemDefinitions;
	TArray<FString> TransferItemNames;
	int32 SelectedTransferItemIndex = 0;
	FString TransferFilterText;
	TArray<FString> FilteredTransferNames;
	TArray<int32> FilteredTransferToSourceIndex;

	// ---- Tab: Recipes ----

	void DrawRecipesTab();
	void DrawRecipeList();
	void DrawRecipeDetail(UArcRecipeDefinition* Recipe);

	TArray<FAssetData> CachedRecipeDefinitions;
	TArray<FString> CachedRecipeNames;
	int32 SelectedRecipeIndex = -1;
	FString RecipeFilterText;
	TArray<FString> FilteredRecipeNames;
	TArray<int32> FilteredRecipeToSourceIndex;

	// ---- Tab: Material Preview ----

	void DrawMaterialPreviewTab();
	void DrawMaterialRecipeSelector();
	void DrawSimulatedIngredients();
	void DrawMaterialContextDisplay(const FArcMaterialCraftContext& Context);
	void DrawRuleEvaluations(const UArcMaterialPropertyTable* Table, const FArcMaterialCraftContext& Context);
	void DrawBandProbabilities(const FArcMaterialPropertyRule& Rule, float BandEligQ, float WeightBonus);
	void DrawOutputPreview();

	// Material recipe list (recipes with FArcRecipeOutputModifier_MaterialProperties)
	TArray<FAssetData> CachedMaterialRecipes;
	TArray<FString> CachedMaterialRecipeNames;
	int32 SelectedMaterialRecipeIndex = -1;
	FString MaterialRecipeFilterText;
	TArray<FString> FilteredMaterialRecipeNames;
	TArray<int32> FilteredMaterialRecipeToSourceIndex;
	bool bMaterialRecipesCached = false;

	// Simulated ingredients per slot
	TArray<int32> SimulatedIngredientDefIndices;

	// Quality override
	float QualityOverride = 1.0f;
	bool bUseQualityOverride = false;

	// Cached evaluation results
	FArcMaterialCraftContext CachedMaterialContext;
	TArray<FArcMaterialRuleEvaluation> CachedEvaluations;
	bool bEvaluationDirty = true;

	// ---- Helpers ----

	void RefreshStations();
	void RefreshRecipeDefinitions();
	void RefreshTransferItemDefinitions();
	void RefreshMaterialRecipes();
	UArcItemsStoreComponent* GetLocalPlayerItemStore() const;
	APlayerController* GetLocalPlayerController() const;

	/** Compute band weight using the documented formula (avoids accessing private method). */
	static float ComputeBandWeight(
		float BaseWeight, float MinQuality, float QualityWeightBias,
		float BandEligibilityQuality, float BandWeightBonus);
};
