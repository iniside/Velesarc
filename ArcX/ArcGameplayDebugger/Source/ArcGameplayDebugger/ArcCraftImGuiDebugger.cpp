// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCraftImGuiDebugger.h"

#include "imgui.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemsStoreComponent.h"
#include "Items/ArcItemSpec.h"
#include "Kismet/GameplayStatics.h"

#include "ArcCraft/Station/ArcCraftStationComponent.h"
#include "ArcCraft/Station/ArcCraftStationTypes.h"
#include "ArcCraft/Recipe/ArcRecipeDefinition.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "ArcCraft/Recipe/ArcRecipeOutput.h"
#include "ArcCraft/MaterialCraft/ArcMaterialCraftContext.h"
#include "ArcCraft/MaterialCraft/ArcMaterialCraftEvaluator.h"
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyTable.h"
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyRule.h"
#include "ArcCraft/MaterialCraft/ArcMaterialOutputModifier.h"
#include "ArcCraft/Shared/ArcCraftModifier.h"
#include "ArcCraft/Recipe/ArcRandomPoolDefinition.h"
#include "ArcCraft/Recipe/ArcRandomPoolSelectionMode.h"
#include "Items/ArcItemTypes.h"
#include "GameplayEffect.h"
#include "Abilities/GameplayAbility.h"

// -------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------

void FArcCraftImGuiDebugger::FilterNames(const TArray<FString>& Source, const char* FilterBuf, TArray<int32>& OutFilteredIndices)
{
	OutFilteredIndices.Reset();
	FString Filter = FString(ANSI_TO_TCHAR(FilterBuf)).ToLower();
	for (int32 i = 0; i < Source.Num(); ++i)
	{
		if (Filter.IsEmpty() || Source[i].ToLower().Contains(Filter))
		{
			OutFilteredIndices.Add(i);
		}
	}
}

void FArcCraftImGuiDebugger::Initialize()
{
}

void FArcCraftImGuiDebugger::Uninitialize()
{
	CachedStations.Reset();
	CachedStationRecipes.Reset();
	CachedRecipeDefinitions.Reset();
	CachedRecipeNames.Reset();
	CachedMaterialRecipes.Reset();
	CachedMaterialRecipeNames.Reset();
	TransferItemDefinitions.Reset();
	TransferItemNames.Reset();
	SimulationResults.Reset();
	CachedEvaluations.Reset();
	CachedPendingModifiers.Reset();
	bMaterialRecipesCached = false;
	bStationRecipesCached = false;
}

void FArcCraftImGuiDebugger::RefreshStations()
{
	CachedStations.Reset();

	if (!World.IsValid())
	{
		World = GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
		if (!World.IsValid())
		{
			return;
		}
	}

	for (TActorIterator<AActor> It(World.Get()); It; ++It)
	{
		AActor* Actor = *It;
		TArray<UArcCraftStationComponent*> Components;
		Actor->GetComponents<UArcCraftStationComponent>(Components);
		for (UArcCraftStationComponent* Comp : Components)
		{
			CachedStations.Add(Comp);
		}
	}
}

void FArcCraftImGuiDebugger::RefreshRecipeDefinitions()
{
	CachedRecipeDefinitions.Reset();
	CachedRecipeNames.Reset();

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	AssetRegistry.GetAssetsByClass(UArcRecipeDefinition::StaticClass()->GetClassPathName(), CachedRecipeDefinitions, true);

	for (const FAssetData& AssetData : CachedRecipeDefinitions)
	{
		CachedRecipeNames.Add(AssetData.AssetName.ToString());
	}
}

void FArcCraftImGuiDebugger::RefreshTransferItemDefinitions()
{
	TransferItemDefinitions.Reset();
	TransferItemNames.Reset();

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	AssetRegistry.GetAssetsByClass(UArcItemDefinition::StaticClass()->GetClassPathName(), TransferItemDefinitions, true);

	for (const FAssetData& AssetData : TransferItemDefinitions)
	{
		TransferItemNames.Add(AssetData.AssetName.ToString());
	}
}

void FArcCraftImGuiDebugger::RefreshMaterialRecipes()
{
	CachedMaterialRecipes.Reset();
	CachedMaterialRecipeNames.Reset();
	bMaterialRecipesCached = true;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AllRecipes;
	AssetRegistry.GetAssetsByClass(UArcRecipeDefinition::StaticClass()->GetClassPathName(), AllRecipes, true);

	for (const FAssetData& AssetData : AllRecipes)
	{
		UArcRecipeDefinition* Recipe = Cast<UArcRecipeDefinition>(AssetData.GetAsset());
		if (!Recipe)
		{
			continue;
		}

		bool bHasMaterialModifier = false;
		for (const FInstancedStruct& Modifier : Recipe->OutputModifiers)
		{
			if (Modifier.GetPtr<FArcRecipeOutputModifier_MaterialProperties>())
			{
				bHasMaterialModifier = true;
				break;
			}
		}

		if (bHasMaterialModifier)
		{
			CachedMaterialRecipes.Add(AssetData);
			CachedMaterialRecipeNames.Add(AssetData.AssetName.ToString());
		}
	}
}

UArcItemsStoreComponent* FArcCraftImGuiDebugger::GetLocalPlayerItemStore() const
{
	if (!World.IsValid())
	{
		World = GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
		if (!World.IsValid())
		{
			return nullptr;
		}
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return nullptr;
	}

	APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		return nullptr;
	}

	TArray<AActor*> ActorsToSearch;
	ActorsToSearch.Add(Pawn);
	ActorsToSearch.Add(PC);
	if (PC->GetPlayerState<APlayerState>())
	{
		ActorsToSearch.Add(PC->GetPlayerState<APlayerState>());
	}

	for (AActor* Actor : ActorsToSearch)
	{
		if (!Actor)
		{
			continue;
		}

		UArcItemsStoreComponent* Store = Actor->FindComponentByClass<UArcItemsStoreComponent>();
		if (Store)
		{
			return Store;
		}
	}

	return nullptr;
}

APlayerController* FArcCraftImGuiDebugger::GetLocalPlayerController() const
{
	if (!World.IsValid())
	{
		World = GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
		if (!World.IsValid())
		{
			return nullptr;
		}
	}

	return World->GetFirstPlayerController();
}

float FArcCraftImGuiDebugger::ComputeBandWeight(
	float BaseWeight, float MinQuality, float QualityWeightBias,
	float BandEligibilityQuality, float BandWeightBonus)
{
	return BaseWeight * (1.0f + (FMath::Max(0.0f, BandEligibilityQuality - MinQuality) + BandWeightBonus) * QualityWeightBias);
}

// -------------------------------------------------------------------
// Draw (main entry point — called from ArcGameplayDebuggerSubsystem)
// -------------------------------------------------------------------

void FArcCraftImGuiDebugger::Draw()
{
	ImGui::SetNextWindowSize(ImVec2(1400.f, 700.f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Arc Craft Debug (ImGui)", &bShow))
	{
		ImGui::End();
		return;
	}

	if (ImGui::BeginTabBar("ArcCraftDebugTabs"))
	{
		if (ImGui::BeginTabItem("Stations"))
		{
			DrawStationsTab();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Recipes"))
		{
			DrawRecipesTab();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Material Preview"))
		{
			DrawMaterialPreviewTab();
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::End();
}

// ===================================================================
// Tab: Stations
// ===================================================================

void FArcCraftImGuiDebugger::DrawStationsTab()
{
	// Toolbar
	if (ImGui::Button("Refresh Stations"))
	{
		RefreshStations();
		SelectedStationIndex = -1;
		bStationRecipesCached = false;
	}

	ImGui::SameLine();
	ImGui::Checkbox("Bypass Ingredients", &bBypassIngredients);

	ImGui::SameLine();
	ImGui::Checkbox("Instant Craft", &bInstantCraft);

	ImGui::Separator();

	// Two-column layout: left list, right detail
	float LeftWidth = 300.f;
	if (ImGui::BeginChild("StationListPanel", ImVec2(LeftWidth, 0.f), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
	{
		DrawStationList();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	if (ImGui::BeginChild("StationDetailPanel", ImVec2(0.f, 0.f)))
	{
		DrawStationDetail();
	}
	ImGui::EndChild();
}

void FArcCraftImGuiDebugger::DrawStationList()
{
	if (CachedStations.Num() == 0)
	{
		ImGui::TextWrapped("No stations found. Click 'Refresh Stations'.");
		return;
	}

	if (ImGui::BeginTable("StationsTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
	{
		ImGui::TableSetupColumn("Owner", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Queue", ImGuiTableColumnFlags_WidthFixed, 60.f);
		ImGui::TableHeadersRow();

		for (int32 i = 0; i < CachedStations.Num(); ++i)
		{
			if (!CachedStations[i].IsValid())
			{
				continue;
			}

			UArcCraftStationComponent* Station = CachedStations[i].Get();

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);

			FString OwnerName = GetNameSafe(Station->GetOwner());
			bool bSelected = (SelectedStationIndex == i);
			if (ImGui::Selectable(TCHAR_TO_ANSI(*OwnerName), bSelected, ImGuiSelectableFlags_SpanAllColumns))
			{
				if (SelectedStationIndex != i)
				{
					SelectedStationIndex = i;
					bStationRecipesCached = false;
					bShowTransferPanel = false;
				}
			}

			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%d", Station->GetQueue().Num());
		}

		ImGui::EndTable();
	}
}

void FArcCraftImGuiDebugger::DrawStationDetail()
{
	if (SelectedStationIndex < 0 || SelectedStationIndex >= CachedStations.Num() ||
		!CachedStations[SelectedStationIndex].IsValid())
	{
		ImGui::Text("Select a station from the list");
		return;
	}

	UArcCraftStationComponent* Station = CachedStations[SelectedStationIndex].Get();

	ImGui::Text("Station: %s", TCHAR_TO_ANSI(*GetNameSafe(Station)));
	ImGui::Text("Owner: %s", TCHAR_TO_ANSI(*GetNameSafe(Station->GetOwner())));
	ImGui::Text("Station Tags: %s", TCHAR_TO_ANSI(*Station->GetStationTags().ToString()));

	ImGui::Spacing();
	DrawCraftQueue(Station);

	ImGui::Spacing();
	DrawAvailableRecipes(Station);

	ImGui::Spacing();
	DrawTransferItemsPanel(Station);

	ImGui::Spacing();
	DrawCraftableDiscovery(Station);
}

void FArcCraftImGuiDebugger::DrawCraftQueue(UArcCraftStationComponent* Station)
{
	ImGui::SeparatorText("Craft Queue");

	const TArray<FArcCraftQueueEntry>& Queue = Station->GetQueue();

	if (Queue.Num() == 0)
	{
		ImGui::TextDisabled("(empty)");
		return;
	}

	if (ImGui::BeginTable("CraftQueue", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
	{
		ImGui::TableSetupColumn("Recipe", ImGuiTableColumnFlags_WidthFixed, 150.f);
		ImGui::TableSetupColumn("Progress", ImGuiTableColumnFlags_WidthFixed, 80.f);
		ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 80.f);
		ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 80.f);
		ImGui::TableHeadersRow();

		for (int32 i = 0; i < Queue.Num(); ++i)
		{
			const FArcCraftQueueEntry& Entry = Queue[i];

			ImGui::TableNextRow();

			// Recipe name
			ImGui::TableSetColumnIndex(0);
			FString RecipeName = Entry.Recipe ? Entry.Recipe->RecipeName.ToString() : TEXT("Unknown");
			ImGui::Text("%s", TCHAR_TO_ANSI(*RecipeName));

			// Progress
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%d / %d", Entry.CompletedAmount, Entry.Amount);

			// Time
			ImGui::TableSetColumnIndex(2);
			if (Entry.Recipe)
			{
				float CraftTime = Entry.Recipe->CraftTime;
				float Progress = (CraftTime > 0.0f) ? FMath::Clamp(Entry.ElapsedTickTime / CraftTime, 0.0f, 1.0f) : 1.0f;
				ImGui::ProgressBar(Progress, ImVec2(-FLT_MIN, 0.f));
			}
			else
			{
				ImGui::Text("-");
			}

			// Actions
			ImGui::TableSetColumnIndex(3);
			ImGui::PushID(i);
			if (ImGui::Button("Cancel"))
			{
				Station->CancelQueueEntry(Entry.EntryId);
			}
			ImGui::PopID();
		}

		ImGui::EndTable();
	}
}

void FArcCraftImGuiDebugger::DrawAvailableRecipes(UArcCraftStationComponent* Station)
{
	ImGui::SeparatorText("Available Recipes");

	if (!bStationRecipesCached)
	{
		CachedStationRecipes = Station->GetAvailableRecipes();
		bStationRecipesCached = true;
	}

	if (CachedStationRecipes.Num() == 0)
	{
		ImGui::TextDisabled("No recipes available for this station.");
		return;
	}

	if (ImGui::BeginTable("AvailableRecipes", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
	{
		ImGui::TableSetupColumn("Recipe", ImGuiTableColumnFlags_WidthFixed, 200.f);
		ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 60.f);
		ImGui::TableSetupColumn("Ingredients", ImGuiTableColumnFlags_WidthFixed, 80.f);
		ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 80.f);
		ImGui::TableHeadersRow();

		APlayerController* PC = GetLocalPlayerController();

		for (int32 i = 0; i < CachedStationRecipes.Num(); ++i)
		{
			UArcRecipeDefinition* Recipe = CachedStationRecipes[i];
			if (!Recipe)
			{
				continue;
			}

			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%s", TCHAR_TO_ANSI(*Recipe->RecipeName.ToString()));

			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%.1fs", Recipe->CraftTime);

			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%d slots", Recipe->GetIngredientCount());

			ImGui::TableSetColumnIndex(3);
			ImGui::PushID(i);
			if (ImGui::Button("Queue"))
			{
				Station->QueueRecipe(Recipe, PC, 1);
				if (bInstantCraft)
				{
					Station->TryCompleteOnInteraction(PC);
				}
			}
			ImGui::PopID();
		}

		ImGui::EndTable();
	}
}

void FArcCraftImGuiDebugger::DrawTransferItemsPanel(UArcCraftStationComponent* Station)
{
	ImGui::SeparatorText("Transfer Items");
	ImGui::SameLine();
	if (ImGui::Button(bShowTransferPanel ? "Hide" : "Show"))
	{
		bShowTransferPanel = !bShowTransferPanel;
		if (bShowTransferPanel && TransferItemDefinitions.Num() == 0)
		{
			RefreshTransferItemDefinitions();
		}
	}

	if (!bShowTransferPanel)
	{
		return;
	}

	ImGui::Indent();

	// Option 1: Transfer from player store
	UArcItemsStoreComponent* PlayerStore = GetLocalPlayerItemStore();
	if (PlayerStore)
	{
		ImGui::Text("Player Store Items:");

		TArray<const FArcItemData*> Items = PlayerStore->GetItems();
		if (Items.Num() == 0)
		{
			ImGui::TextDisabled("(no items in player store)");
		}
		else
		{
			if (ImGui::BeginChild("PlayerStoreItems", ImVec2(0.f, 150.f), ImGuiChildFlags_Borders))
			{
				for (int32 i = 0; i < Items.Num(); ++i)
				{
					const FArcItemData* Item = Items[i];
					if (!Item)
					{
						continue;
					}

					const UArcItemDefinition* Def = Item->GetItemDefinition();
					ImGui::Text("%s (L%d x%d)",
						TCHAR_TO_ANSI(*GetNameSafe(Def)), Item->GetLevel(), Item->GetStacks());

					ImGui::SameLine();
					ImGui::PushID(i);
					if (ImGui::SmallButton("Deposit"))
					{
						APlayerController* PC = GetLocalPlayerController();
						FArcItemSpec Spec = FArcItemSpec::NewItem(
							const_cast<UArcItemDefinition*>(Def), Item->GetStacks(), Item->GetLevel());
						Station->DepositItem(Spec, PC);
					}
					ImGui::PopID();
				}
			}
			ImGui::EndChild();
		}
	}
	else
	{
		ImGui::Text("No player item store found.");
	}

	ImGui::Spacing();

	// Option 2: Spawn item from definition list
	ImGui::Text("Spawn from Definition:");

	ImGui::Text("Filter:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(200.f);
	ImGui::InputText("##TransferFilter", TransferFilterBuf, sizeof(TransferFilterBuf));

	ImGui::SameLine();
	if (ImGui::Button("Refresh Defs"))
	{
		RefreshTransferItemDefinitions();
		TransferFilterBuf[0] = '\0';
	}

	TArray<int32> FilteredTransferIndices;
	FilterNames(TransferItemNames, TransferFilterBuf, FilteredTransferIndices);

	if (FilteredTransferIndices.Num() > 0)
	{
		if (ImGui::BeginChild("TransferDefList", ImVec2(0.f, 120.f), ImGuiChildFlags_Borders))
		{
			for (int32 FilterIdx = 0; FilterIdx < FilteredTransferIndices.Num(); ++FilterIdx)
			{
				int32 SourceIdx = FilteredTransferIndices[FilterIdx];
				bool bSelected = (SelectedTransferItemIndex == SourceIdx);
				if (ImGui::Selectable(TCHAR_TO_ANSI(*TransferItemNames[SourceIdx]), bSelected))
				{
					SelectedTransferItemIndex = SourceIdx;
				}
			}
		}
		ImGui::EndChild();

		if (ImGui::Button("Deposit Selected"))
		{
			if (SelectedTransferItemIndex >= 0 && SelectedTransferItemIndex < TransferItemDefinitions.Num())
			{
				UArcItemDefinition* Def = Cast<UArcItemDefinition>(TransferItemDefinitions[SelectedTransferItemIndex].GetAsset());
				if (Def)
				{
					APlayerController* PC = GetLocalPlayerController();
					FArcItemSpec Spec = FArcItemSpec::NewItem(Def, 1, 1);
					Station->DepositItem(Spec, PC);
				}
			}
		}
	}

	ImGui::Unindent();
}

void FArcCraftImGuiDebugger::DrawCraftableDiscovery(UArcCraftStationComponent* Station)
{
	ImGui::SeparatorText("Craftable Discovery");
	ImGui::TextWrapped("Recipes that can be crafted with currently available ingredients:");

	APlayerController* PC = GetLocalPlayerController();
	TArray<UArcRecipeDefinition*> Craftable = Station->GetCraftableRecipes(PC);

	if (Craftable.Num() == 0)
	{
		ImGui::TextDisabled("(none - deposit more items or check station tags)");
		return;
	}

	for (int32 i = 0; i < Craftable.Num(); ++i)
	{
		UArcRecipeDefinition* Recipe = Craftable[i];
		if (!Recipe)
		{
			continue;
		}

		ImGui::Text("%s (%.1fs, %d ingredients)",
			TCHAR_TO_ANSI(*Recipe->RecipeName.ToString()),
			Recipe->CraftTime,
			Recipe->GetIngredientCount());

		ImGui::SameLine();
		ImGui::PushID(i);
		if (ImGui::SmallButton("Queue"))
		{
			Station->QueueRecipe(Recipe, PC, 1);
			if (bInstantCraft)
			{
				Station->TryCompleteOnInteraction(PC);
			}
		}
		ImGui::PopID();
	}
}

// ===================================================================
// Tab: Recipes
// ===================================================================

void FArcCraftImGuiDebugger::DrawRecipesTab()
{
	float LeftWidth = 280.f;
	if (ImGui::BeginChild("RecipeListPanel", ImVec2(LeftWidth, 0.f), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
	{
		DrawRecipeList();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	if (ImGui::BeginChild("RecipeDetailPanel", ImVec2(0.f, 0.f)))
	{
		TArray<int32> FilteredIndices;
		FilterNames(CachedRecipeNames, RecipeFilterBuf, FilteredIndices);

		if (SelectedRecipeIndex >= 0 && SelectedRecipeIndex < FilteredIndices.Num())
		{
			int32 SourceIdx = FilteredIndices[SelectedRecipeIndex];
			if (SourceIdx >= 0 && SourceIdx < CachedRecipeDefinitions.Num())
			{
				UArcRecipeDefinition* Recipe = Cast<UArcRecipeDefinition>(
					CachedRecipeDefinitions[SourceIdx].GetAsset());
				if (Recipe)
				{
					DrawRecipeDetail(Recipe);
				}
			}
		}
		else
		{
			ImGui::Text("Select a recipe to view details");
		}
	}
	ImGui::EndChild();
}

void FArcCraftImGuiDebugger::DrawRecipeList()
{
	if (ImGui::Button("Refresh"))
	{
		RefreshRecipeDefinitions();
		SelectedRecipeIndex = -1;
	}

	ImGui::Text("Filter:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputText("##RecipeFilter", RecipeFilterBuf, sizeof(RecipeFilterBuf));

	if (CachedRecipeNames.Num() == 0)
	{
		ImGui::TextWrapped("Click 'Refresh' to load recipe definitions.");
		return;
	}

	TArray<int32> FilteredIndices;
	FilterNames(CachedRecipeNames, RecipeFilterBuf, FilteredIndices);

	if (ImGui::BeginChild("RecipeSelectionList"))
	{
		for (int32 FilterIdx = 0; FilterIdx < FilteredIndices.Num(); ++FilterIdx)
		{
			int32 SourceIdx = FilteredIndices[FilterIdx];
			bool bSelected = (SelectedRecipeIndex == FilterIdx);
			if (ImGui::Selectable(TCHAR_TO_ANSI(*CachedRecipeNames[SourceIdx]), bSelected))
			{
				SelectedRecipeIndex = FilterIdx;
			}
		}
	}
	ImGui::EndChild();
}

void FArcCraftImGuiDebugger::DrawRecipeDetail(UArcRecipeDefinition* Recipe)
{
	if (!Recipe)
	{
		return;
	}

	if (ImGui::BeginChild("RecipeDetailScroll"))
	{
		// Basic info
		ImGui::Text("Name: %s", TCHAR_TO_ANSI(*Recipe->RecipeName.ToString()));
		ImGui::Text("ID: %s", TCHAR_TO_ANSI(*Recipe->RecipeId.ToString()));
		ImGui::Text("Craft Time: %.1f s", Recipe->CraftTime);

		if (Recipe->RecipeTags.Num() > 0)
		{
			ImGui::Text("Recipe Tags: %s", TCHAR_TO_ANSI(*Recipe->RecipeTags.ToString()));
		}

		// Requirements
		ImGui::Spacing();
		ImGui::SeparatorText("Requirements");
		if (Recipe->RequiredStationTags.Num() > 0)
		{
			ImGui::Text("Station Tags: %s", TCHAR_TO_ANSI(*Recipe->RequiredStationTags.ToString()));
		}
		else
		{
			ImGui::Text("Station Tags: (none)");
		}
		if (Recipe->RequiredInstigatorTags.Num() > 0)
		{
			ImGui::Text("Instigator Tags: %s", TCHAR_TO_ANSI(*Recipe->RequiredInstigatorTags.ToString()));
		}

		// Ingredients
		ImGui::Spacing();
		ImGui::SeparatorText("Ingredients");
		for (int32 i = 0; i < Recipe->GetIngredientCount(); ++i)
		{
			const FArcRecipeIngredient* Base = Recipe->GetIngredientBase(i);
			if (!Base)
			{
				continue;
			}

			FString IngredientInfo = FString::Printf(TEXT("  [%d] %s x%d%s"),
				i,
				*Base->SlotName.ToString(),
				Base->Amount,
				Base->bConsumeOnCraft ? TEXT("") : TEXT(" (not consumed)"));

			if (const FArcRecipeIngredient_ItemDef* ItemDefIng = Recipe->GetIngredient<FArcRecipeIngredient_ItemDef>(i))
			{
				IngredientInfo += FString::Printf(TEXT(" [Def: %s]"), *ItemDefIng->ItemDefinitionId.ToString());
			}
			else if (const FArcRecipeIngredient_Tags* TagIng = Recipe->GetIngredient<FArcRecipeIngredient_Tags>(i))
			{
				if (TagIng->RequiredTags.Num() > 0)
				{
					IngredientInfo += FString::Printf(TEXT(" [Tags: %s]"), *TagIng->RequiredTags.ToString());
				}
				if (TagIng->DenyTags.Num() > 0)
				{
					IngredientInfo += FString::Printf(TEXT(" [Deny: %s]"), *TagIng->DenyTags.ToString());
				}
			}

			ImGui::TextWrapped("%s", TCHAR_TO_ANSI(*IngredientInfo));
		}

		// Output
		ImGui::Spacing();
		ImGui::SeparatorText("Output");
		ImGui::Text("Item: %s", TCHAR_TO_ANSI(*Recipe->OutputItemDefinition.ToString()));
		ImGui::Text("Amount: %d", Recipe->OutputAmount);
		ImGui::Text("Level: %d", Recipe->OutputLevel);

		// Output modifiers
		if (Recipe->OutputModifiers.Num() > 0)
		{
			ImGui::Spacing();
			ImGui::SeparatorText("Output Modifiers");
			for (int32 i = 0; i < Recipe->OutputModifiers.Num(); ++i)
			{
				const FInstancedStruct& Modifier = Recipe->OutputModifiers[i];
				if (Modifier.IsValid())
				{
					const UScriptStruct* Struct = Modifier.GetScriptStruct();
					ImGui::Text("  [%d] %s", i, TCHAR_TO_ANSI(*GetNameSafe(Struct)));
				}
			}
		}

		// Quality
		ImGui::Spacing();
		ImGui::SeparatorText("Quality");
		ImGui::Text("Quality Affects Level: %s", Recipe->bQualityAffectsLevel ? "Yes" : "No");
		if (!Recipe->QualityTierTable.IsNull())
		{
			ImGui::Text("Tier Table: %s", TCHAR_TO_ANSI(*Recipe->QualityTierTable.GetAssetName()));
		}
	}
	ImGui::EndChild();
}

// ===================================================================
// Tab: Material Preview
// ===================================================================

void FArcCraftImGuiDebugger::DrawMaterialPreviewTab()
{
	// Recipe selector (always visible at top)
	DrawMaterialRecipeSelector();

	// Only show the rest if a recipe with material modifier is selected
	TArray<int32> FilteredIndices;
	FilterNames(CachedMaterialRecipeNames, MaterialRecipeFilterBuf, FilteredIndices);

	if (SelectedMaterialRecipeIndex < 0 || SelectedMaterialRecipeIndex >= FilteredIndices.Num())
	{
		return;
	}

	int32 SourceIdx = FilteredIndices[SelectedMaterialRecipeIndex];
	if (SourceIdx < 0 || SourceIdx >= CachedMaterialRecipes.Num())
	{
		return;
	}

	UArcRecipeDefinition* Recipe = Cast<UArcRecipeDefinition>(CachedMaterialRecipes[SourceIdx].GetAsset());
	if (!Recipe)
	{
		return;
	}

	// Find the material properties modifier
	const FArcRecipeOutputModifier_MaterialProperties* MatMod = nullptr;
	for (const FInstancedStruct& Modifier : Recipe->OutputModifiers)
	{
		MatMod = Modifier.GetPtr<FArcRecipeOutputModifier_MaterialProperties>();
		if (MatMod)
		{
			break;
		}
	}

	if (!MatMod)
	{
		return;
	}

	UArcMaterialPropertyTable* Table = MatMod->PropertyTable.LoadSynchronous();

	ImGui::Spacing();
	DrawSimulatedIngredients();

	// Build context and evaluate
	if (bEvaluationDirty && Table)
	{
		TArray<const FArcItemData*> SimIngredients;
		TArray<float> QualityMults;
		float AvgQuality = 1.0f;

		int32 SlotCount = Recipe->GetIngredientCount();
		for (int32 i = 0; i < SlotCount; ++i)
		{
			SimIngredients.Add(nullptr);
			QualityMults.Add(1.0f);
		}

		AvgQuality = bUseQualityOverride ? QualityOverride : 1.0f;

		CachedMaterialContext = FArcMaterialCraftContext::Build(
			SimIngredients,
			QualityMults,
			AvgQuality,
			MatMod->RecipeTags,
			MatMod->BaseIngredientCount,
			MatMod->ExtraCraftTimeBonus);

		if (bUsePreviewTags && PreviewTags.Num() > 0)
		{
			if (CachedMaterialContext.PerSlotTags.Num() == 0)
			{
				CachedMaterialContext.PerSlotTags.AddDefaulted();
			}
			CachedMaterialContext.PerSlotTags[0].AppendTags(PreviewTags);
		}

		FArcMaterialCraftEvaluator::ComputeQualityAndWeightBonus(Table, CachedMaterialContext);

		if (bUseQualityOverride)
		{
			CachedMaterialContext.BandEligibilityQuality = QualityOverride;
		}

		CachedEvaluations = FArcMaterialCraftEvaluator::EvaluateRules(Table, CachedMaterialContext);

		// Build pending modifiers from evaluations
		CachedPendingModifiers.Reset();
		const float BandEligQ = CachedMaterialContext.BandEligibilityQuality;
		for (const FArcMaterialRuleEvaluation& Eval : CachedEvaluations)
		{
			if (!Eval.Band || !Eval.Rule)
			{
				continue;
			}

			FGameplayTag EvalSlotTag = MatMod->SlotTag;
			if (!Eval.Rule->OutputTags.IsEmpty())
			{
				auto TagIt = Eval.Rule->OutputTags.CreateConstIterator();
				EvalSlotTag = *TagIt;
			}

			for (const FInstancedStruct& ModStruct : Eval.Band->Modifiers)
			{
				const FArcCraftModifier* BaseMod = ModStruct.GetPtr<FArcCraftModifier>();
				if (!BaseMod)
				{
					continue;
				}

				if (BaseMod->MinQualityThreshold > 0.0f && BandEligQ < BaseMod->MinQualityThreshold)
				{
					continue;
				}

				FArcCraftModifierResult ModResult;
				if (const FArcCraftModifier_Stats* StatMod = ModStruct.GetPtr<FArcCraftModifier_Stats>())
				{
					const float QScale = 1.0f + (BandEligQ - 1.0f) * StatMod->QualityScalingFactor;
					ModResult.Type = EArcCraftModifierResultType::Stat;
					ModResult.Stat = StatMod->BaseStat;
					ModResult.Stat.SetValue(StatMod->BaseStat.Value.GetValue() * QScale);
				}
				else if (const FArcCraftModifier_Abilities* AbilMod = ModStruct.GetPtr<FArcCraftModifier_Abilities>())
				{
					ModResult.Type = EArcCraftModifierResultType::Ability;
					ModResult.Ability = AbilMod->AbilityToGrant;
				}
				else if (const FArcCraftModifier_Effects* EffMod = ModStruct.GetPtr<FArcCraftModifier_Effects>())
				{
					ModResult.Type = EArcCraftModifierResultType::Effect;
					ModResult.Effect = EffMod->EffectToGrant;
				}
				else
				{
					continue;
				}

				FArcCraftPendingModifier Pending;
				Pending.SlotTag = EvalSlotTag;
				Pending.EffectiveWeight = Eval.EffectiveWeight;
				Pending.Result = MoveTemp(ModResult);
				CachedPendingModifiers.Add(MoveTemp(Pending));
			}
		}

		bEvaluationDirty = false;
	}

	ImGui::Spacing();

	// Quality override
	if (ImGui::Checkbox("Quality Override", &bUseQualityOverride))
	{
		bEvaluationDirty = true;
		bSimulationDirty = true;
	}

	if (bUseQualityOverride)
	{
		ImGui::SameLine();
		ImGui::SetNextItemWidth(200.f);
		if (ImGui::SliderFloat("##QualitySlider", &QualityOverride, 0.0f, 5.0f, "%.2f"))
		{
			bEvaluationDirty = true;
			bSimulationDirty = true;
		}
	}

	// Preview tags
	if (ImGui::Checkbox("Preview Tags", &bUsePreviewTags))
	{
		bEvaluationDirty = true;
		bSimulationDirty = true;
	}

	if (bUsePreviewTags)
	{
		ImGui::SameLine();
		ImGui::SetNextItemWidth(360.f);
		if (ImGui::InputText("##PreviewTags", PreviewTagsBuf, sizeof(PreviewTagsBuf)))
		{
			PreviewTags.Reset();
			TArray<FString> TagStrings;
			FString(ANSI_TO_TCHAR(PreviewTagsBuf)).ParseIntoArray(TagStrings, TEXT(","));
			for (FString& TagStr : TagStrings)
			{
				TagStr.TrimStartAndEndInline();
				if (!TagStr.IsEmpty())
				{
					FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagStr), false);
					if (Tag.IsValid())
					{
						PreviewTags.AddTag(Tag);
					}
				}
			}
			bEvaluationDirty = true;
			bSimulationDirty = true;
		}
		ImGui::SetItemTooltip("Comma-separated tags, e.g. Resource.Metal.Iron, Resource.Gem");

		if (PreviewTags.Num() > 0)
		{
			ImGui::Indent();
			ImGui::Text("Active preview tags: %s", TCHAR_TO_ANSI(*PreviewTags.ToStringSimple()));
			ImGui::Unindent();
		}
		else
		{
			ImGui::Indent();
			ImGui::TextDisabled("(no valid tags parsed - type registered gameplay tags)");
			ImGui::Unindent();
		}
	}

	// Re-roll button
	if (ImGui::Button("Re-roll"))
	{
		bEvaluationDirty = true;
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// Two-column layout
	float LeftWidth = ImGui::GetContentRegionAvail().x * 0.5f;
	if (ImGui::BeginChild("MaterialLeftCol", ImVec2(LeftWidth, 0.f)))
	{
		DrawOutputPreview();
		ImGui::Spacing();
		DrawPendingModifiersTable();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	if (ImGui::BeginChild("MaterialRightCol", ImVec2(0.f, 0.f)))
	{
		DrawMaterialContextDisplay(CachedMaterialContext);

		if (Table)
		{
			ImGui::Spacing();
			DrawRuleEvaluations(Table, CachedMaterialContext);
		}

		ImGui::Spacing();
		DrawSimulationPanel(Table, CachedMaterialContext);
	}
	ImGui::EndChild();
}

void FArcCraftImGuiDebugger::DrawMaterialRecipeSelector()
{
	ImGui::SeparatorText("Material Recipe");

	if (ImGui::Button("Refresh##MatRecipe"))
	{
		RefreshMaterialRecipes();
		SelectedMaterialRecipeIndex = -1;
		bEvaluationDirty = true;
		bSimulationDirty = true;
	}

	ImGui::SameLine();
	ImGui::Text("Filter:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(180.f);
	if (ImGui::InputText("##MatRecipeFilter", MaterialRecipeFilterBuf, sizeof(MaterialRecipeFilterBuf)))
	{
		SelectedMaterialRecipeIndex = -1;
		bEvaluationDirty = true;
		bSimulationDirty = true;
	}

	if (!bMaterialRecipesCached)
	{
		ImGui::TextWrapped("Click 'Refresh' to load recipes with material properties.");
		return;
	}

	TArray<int32> FilteredIndices;
	FilterNames(CachedMaterialRecipeNames, MaterialRecipeFilterBuf, FilteredIndices);

	if (FilteredIndices.Num() == 0)
	{
		ImGui::TextDisabled("No recipes with Material Properties modifier found.");
		return;
	}

	if (ImGui::BeginChild("MatRecipeList", ImVec2(0.f, 120.f), ImGuiChildFlags_Borders))
	{
		int32 PrevIndex = SelectedMaterialRecipeIndex;
		for (int32 FilterIdx = 0; FilterIdx < FilteredIndices.Num(); ++FilterIdx)
		{
			int32 SourceIdx = FilteredIndices[FilterIdx];
			bool bSelected = (SelectedMaterialRecipeIndex == FilterIdx);
			if (ImGui::Selectable(TCHAR_TO_ANSI(*CachedMaterialRecipeNames[SourceIdx]), bSelected))
			{
				SelectedMaterialRecipeIndex = FilterIdx;
			}
		}
		if (SelectedMaterialRecipeIndex != PrevIndex)
		{
			bEvaluationDirty = true;
			bSimulationDirty = true;
		}
	}
	ImGui::EndChild();
}

void FArcCraftImGuiDebugger::DrawSimulatedIngredients()
{
	ImGui::SeparatorText("Simulated Ingredients");
	ImGui::TextDisabled("(Ingredient simulation simplified - uses per-slot quality via override slider)");
}

void FArcCraftImGuiDebugger::DrawMaterialContextDisplay(const FArcMaterialCraftContext& Context)
{
	ImGui::SeparatorText("Evaluation Context");

	ImGui::Text("Average Quality: %.3f", Context.AverageQuality);
	ImGui::Text("Band Eligibility Quality: %.3f", Context.BandEligibilityQuality);
	ImGui::Text("Band Weight Bonus: %.3f", Context.BandWeightBonus);
	ImGui::Text("Ingredient Count: %d (base: %d)", Context.IngredientCount, Context.BaseIngredientCount);
	ImGui::Text("Extra Craft Time Bonus: %.3f", Context.ExtraCraftTimeBonus);

	if (Context.RecipeTags.Num() > 0)
	{
		ImGui::Text("Recipe Tags: %s", TCHAR_TO_ANSI(*Context.RecipeTags.ToString()));
	}

	if (Context.PerSlotTags.Num() > 0)
	{
		ImGui::Text("Per-Slot Tags: %d slots", Context.PerSlotTags.Num());
		for (int32 i = 0; i < Context.PerSlotTags.Num(); ++i)
		{
			if (Context.PerSlotTags[i].Num() > 0)
			{
				ImGui::Text("  Slot %d: %s", i, TCHAR_TO_ANSI(*Context.PerSlotTags[i].ToString()));
			}
		}
	}
}

void FArcCraftImGuiDebugger::DrawRuleEvaluations(
	const UArcMaterialPropertyTable* Table,
	const FArcMaterialCraftContext& Context)
{
	ImGui::SeparatorText("Rule Evaluations");

	if (!Table)
	{
		ImGui::TextDisabled("(no property table)");
		return;
	}

	ImGui::Text("Table: %s (%d rules, MaxActive: %d)",
		TCHAR_TO_ANSI(*Table->TableName.ToString()),
		Table->Rules.Num(),
		Table->MaxActiveRules);

	if (Table->BaseBandBudget > 0.0f)
	{
		float TotalBudget = Table->BaseBandBudget +
			FMath::Max(0.0f, Context.BandEligibilityQuality - 1.0f) * Table->BudgetPerQuality;
		ImGui::Text("Band Budget: %.1f (base: %.1f, per quality: %.1f)",
			TotalBudget, Table->BaseBandBudget, Table->BudgetPerQuality);
	}

	ImGui::Text("Matched Rules: %d", CachedEvaluations.Num());
	ImGui::Spacing();

	// Show matched evaluations
	for (int32 EvalIdx = 0; EvalIdx < CachedEvaluations.Num(); ++EvalIdx)
	{
		const FArcMaterialRuleEvaluation& Eval = CachedEvaluations[EvalIdx];
		if (!Eval.Rule)
		{
			continue;
		}

		const FArcMaterialPropertyRule& Rule = *Eval.Rule;
		FString RuleName = Rule.RuleName.IsEmpty() ? FString::Printf(TEXT("Rule %d"), Eval.RuleIndex) : Rule.RuleName.ToString();

		ImGui::PushID(EvalIdx);
		if (ImGui::TreeNodeEx(TCHAR_TO_ANSI(*FString::Printf(TEXT("%s (Priority: %d)"), *RuleName, Rule.Priority)),
			ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (!Rule.TagQuery.IsEmpty())
			{
				ImGui::Text("Tag Query: %s", TCHAR_TO_ANSI(*Rule.TagQuery.GetDescription()));
			}
			else
			{
				ImGui::Text("Tag Query: (any - no tag requirement)");
			}

			if (!Rule.RequiredRecipeTags.IsEmpty())
			{
				ImGui::Text("Required Recipe Tags: %s", TCHAR_TO_ANSI(*Rule.RequiredRecipeTags.ToStringSimple()));
			}

			if (Eval.Band)
			{
				FString BandName = Eval.Band->BandName.IsEmpty()
					? FString::Printf(TEXT("Band %d"), Eval.SelectedBandIndex)
					: Eval.Band->BandName.ToString();
				ImGui::Text("Selected Band: %s (index %d)", TCHAR_TO_ANSI(*BandName), Eval.SelectedBandIndex);
				ImGui::Text("Effective Weight: %.3f", Eval.EffectiveWeight);
			}

			if (Rule.OutputTags.Num() > 0)
			{
				ImGui::Text("Output Tags: %s", TCHAR_TO_ANSI(*Rule.OutputTags.ToString()));
			}

			// Band probabilities
			DrawBandProbabilities(Rule, Context.BandEligibilityQuality, Context.BandWeightBonus);

			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	// Build combined tags for match diagnostics
	FGameplayTagContainer CombinedIngredientTags;
	for (const FGameplayTagContainer& SlotTags : Context.PerSlotTags)
	{
		CombinedIngredientTags.AppendTags(SlotTags);
	}

	// All rules in table
	ImGui::Spacing();
	if (ImGui::TreeNode("All Rules in Table"))
	{
		for (int32 RuleIdx = 0; RuleIdx < Table->Rules.Num(); ++RuleIdx)
		{
			const FArcMaterialPropertyRule& Rule = Table->Rules[RuleIdx];
			FString RuleName = Rule.RuleName.IsEmpty()
				? FString::Printf(TEXT("Rule %d"), RuleIdx)
				: Rule.RuleName.ToString();

			bool bMatched = false;
			for (const FArcMaterialRuleEvaluation& Eval : CachedEvaluations)
			{
				if (Eval.RuleIndex == RuleIdx)
				{
					bMatched = true;
					break;
				}
			}

			bool bTagQueryPass = Rule.TagQuery.IsEmpty() || Rule.TagQuery.Matches(CombinedIngredientTags);
			bool bRecipeTagsPass = Rule.RequiredRecipeTags.IsEmpty() || Context.RecipeTags.HasAll(Rule.RequiredRecipeTags);

			if (bMatched)
			{
				ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "  [%d] %s (Priority: %d) [MATCHED]",
					RuleIdx, TCHAR_TO_ANSI(*RuleName), Rule.Priority);
			}
			else
			{
				ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "  [%d] %s (Priority: %d) [not matched]",
					RuleIdx, TCHAR_TO_ANSI(*RuleName), Rule.Priority);
			}

			// Tag Query
			if (!Rule.TagQuery.IsEmpty())
			{
				ImGui::Text("      TagQuery: %s  [%s]",
					TCHAR_TO_ANSI(*Rule.TagQuery.GetDescription()),
					bTagQueryPass ? "PASS" : "NO MATCH");
			}
			else
			{
				ImGui::Text("      TagQuery: (any)  [PASS]");
			}

			// Required Recipe Tags
			if (!Rule.RequiredRecipeTags.IsEmpty())
			{
				ImGui::Text("      RequiredRecipeTags: %s  [%s]",
					TCHAR_TO_ANSI(*Rule.RequiredRecipeTags.ToStringSimple()),
					bRecipeTagsPass ? "PASS" : "NO MATCH");
			}
		}
		ImGui::TreePop();
	}
}

void FArcCraftImGuiDebugger::DrawBandProbabilities(
	const FArcMaterialPropertyRule& Rule,
	float BandEligQ,
	float WeightBonus)
{
	const TArray<FArcMaterialQualityBand>& Bands = Rule.GetEffectiveQualityBands();

	if (Bands.Num() == 0)
	{
		ImGui::TextDisabled("(no quality bands)");
		return;
	}

	// Compute eligible bands and weights
	TArray<int32> EligibleIndices;
	TArray<float> Weights;
	float TotalWeight = 0.0f;

	for (int32 i = 0; i < Bands.Num(); ++i)
	{
		const FArcMaterialQualityBand& Band = Bands[i];
		if (BandEligQ >= Band.MinQuality)
		{
			float EffWeight = ComputeBandWeight(
				Band.BaseWeight, Band.MinQuality, Band.QualityWeightBias,
				BandEligQ, WeightBonus);
			EligibleIndices.Add(i);
			Weights.Add(EffWeight);
			TotalWeight += EffWeight;
		}
	}

	ImGui::Text("Band Probabilities (Eligible: %d / %d):", EligibleIndices.Num(), Bands.Num());

	if (ImGui::BeginTable("BandProbs", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
	{
		ImGui::TableSetupColumn("Band", 0, 120.f);
		ImGui::TableSetupColumn("MinQ", 0, 50.f);
		ImGui::TableSetupColumn("BaseW", 0, 50.f);
		ImGui::TableSetupColumn("Bias", 0, 50.f);
		ImGui::TableSetupColumn("EffWeight", 0, 70.f);
		ImGui::TableSetupColumn("Prob %", 0, 100.f);
		ImGui::TableHeadersRow();

		// Eligible bands
		for (int32 EligIdx = 0; EligIdx < EligibleIndices.Num(); ++EligIdx)
		{
			int32 BandIdx = EligibleIndices[EligIdx];
			const FArcMaterialQualityBand& Band = Bands[BandIdx];
			float EffWeight = Weights[EligIdx];
			float Probability = (TotalWeight > 0.0f) ? (EffWeight / TotalWeight * 100.0f) : 0.0f;

			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			FString BandName = Band.BandName.IsEmpty()
				? FString::Printf(TEXT("Band %d"), BandIdx)
				: Band.BandName.ToString();
			ImGui::Text("%s", TCHAR_TO_ANSI(*BandName));

			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%.2f", Band.MinQuality);

			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%.2f", Band.BaseWeight);

			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%.2f", Band.QualityWeightBias);

			ImGui::TableSetColumnIndex(4);
			ImGui::Text("%.3f", EffWeight);

			ImGui::TableSetColumnIndex(5);
			ImGui::ProgressBar(Probability / 100.0f, ImVec2(-FLT_MIN, 0.f),
				TCHAR_TO_ANSI(*FString::Printf(TEXT("%.1f%%"), Probability)));
		}

		// Ineligible bands
		for (int32 BandIdx = 0; BandIdx < Bands.Num(); ++BandIdx)
		{
			if (EligibleIndices.Contains(BandIdx))
			{
				continue;
			}

			const FArcMaterialQualityBand& Band = Bands[BandIdx];

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			FString BandName = Band.BandName.IsEmpty()
				? FString::Printf(TEXT("Band %d"), BandIdx)
				: Band.BandName.ToString();
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s (ineligible)", TCHAR_TO_ANSI(*BandName));

			ImGui::TableSetColumnIndex(1);
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%.2f", Band.MinQuality);

			ImGui::TableSetColumnIndex(2);
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%.2f", Band.BaseWeight);

			ImGui::TableSetColumnIndex(3);
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%.2f", Band.QualityWeightBias);

			ImGui::TableSetColumnIndex(4);
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "-");

			ImGui::TableSetColumnIndex(5);
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "-");
		}

		ImGui::EndTable();
	}

	// Show modifier details for each eligible band
	for (int32 EligIdx = 0; EligIdx < EligibleIndices.Num(); ++EligIdx)
	{
		int32 BandIdx = EligibleIndices[EligIdx];
		const FArcMaterialQualityBand& Band = Bands[BandIdx];
		FString BandName = Band.BandName.IsEmpty()
			? FString::Printf(TEXT("Band %d"), BandIdx)
			: Band.BandName.ToString();

		ImGui::PushID(EligIdx);
		if (ImGui::TreeNode(TCHAR_TO_ANSI(*FString::Printf(TEXT("Modifiers in %s"), *BandName))))
		{
			DrawBandModifierDetails(Band, BandEligQ);
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
}

void FArcCraftImGuiDebugger::DrawBandModifierDetails(
	const FArcMaterialQualityBand& Band,
	float BandEligQ)
{
	if (Band.Modifiers.Num() == 0)
	{
		ImGui::TextDisabled("(no modifiers)");
		return;
	}

	for (int32 ModIdx = 0; ModIdx < Band.Modifiers.Num(); ++ModIdx)
	{
		const FInstancedStruct& ModStruct = Band.Modifiers[ModIdx];
		if (!ModStruct.IsValid())
		{
			continue;
		}

		const FArcCraftModifier* BaseMod = ModStruct.GetPtr<FArcCraftModifier>();
		if (!BaseMod)
		{
			continue;
		}

		// Determine type name
		const UScriptStruct* Struct = ModStruct.GetScriptStruct();
		FString TypeName = GetNameSafe(Struct);
		if (TypeName.StartsWith(TEXT("ArcCraftModifier_")))
		{
			TypeName = TypeName.RightChop(17);
		}

		const bool bPassesQualityGate = (BaseMod->MinQualityThreshold <= 0.0f) || (BandEligQ >= BaseMod->MinQualityThreshold);

		if (bPassesQualityGate)
		{
			ImGui::Text("[%d] %s  |  QualityGate: PASS (min: %.2f)  |  QScale: %.2f  |  Weight: %.2f",
				ModIdx, TCHAR_TO_ANSI(*TypeName), BaseMod->MinQualityThreshold, BaseMod->QualityScalingFactor, BaseMod->Weight);
		}
		else
		{
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
				"[%d] %s  |  QualityGate: FAIL (min: %.2f)  |  QScale: %.2f  |  Weight: %.2f",
				ModIdx, TCHAR_TO_ANSI(*TypeName), BaseMod->MinQualityThreshold, BaseMod->QualityScalingFactor, BaseMod->Weight);
		}

		if (!BaseMod->TriggerTags.IsEmpty())
		{
			ImGui::Text("    TriggerTags: %s  (N/A in preview)", TCHAR_TO_ANSI(*BaseMod->TriggerTags.ToString()));
		}

		// Type-specific details
		if (const FArcCraftModifier_Stats* StatsMod = ModStruct.GetPtr<FArcCraftModifier_Stats>())
		{
			const float QualityScale = 1.0f + (BandEligQ - 1.0f) * StatsMod->QualityScalingFactor;
			const float BaseVal = StatsMod->BaseStat.Value.GetValue();
			const float ScaledVal = BaseVal * QualityScale;
			FString AttrName = StatsMod->BaseStat.Attribute.IsValid() ? StatsMod->BaseStat.Attribute.GetName() : TEXT("(none)");
			ImGui::Text("      %s: %.2f -> %.2f (x%.3f)", TCHAR_TO_ANSI(*AttrName), BaseVal, ScaledVal, QualityScale);
		}
		else if (const FArcCraftModifier_Abilities* AbilMod = ModStruct.GetPtr<FArcCraftModifier_Abilities>())
		{
			ImGui::Text("      Ability: %s", TCHAR_TO_ANSI(*GetNameSafe(AbilMod->AbilityToGrant.GrantedAbility)));
		}
		else if (const FArcCraftModifier_Effects* EffMod = ModStruct.GetPtr<FArcCraftModifier_Effects>())
		{
			ImGui::Text("      Effect: %s", TCHAR_TO_ANSI(*GetNameSafe(EffMod->EffectToGrant.Get())));
		}
		else if (const FArcCraftModifier_RandomPool* PoolMod = ModStruct.GetPtr<FArcCraftModifier_RandomPool>())
		{
			UArcRandomPoolDefinition* PoolDef = PoolMod->PoolDefinition.LoadSynchronous();
			if (PoolDef)
			{
				ImGui::Text("      Pool: %s (%d entries)",
					TCHAR_TO_ANSI(*PoolDef->PoolName.ToString()), PoolDef->Entries.Num());

				if (PoolMod->SelectionMode.IsValid())
				{
					const UScriptStruct* ModeStruct = PoolMod->SelectionMode.GetScriptStruct();
					FString ModeName = GetNameSafe(ModeStruct);
					if (ModeName.StartsWith(TEXT("ArcRandomPoolSelection_")))
					{
						ModeName = ModeName.RightChop(24);
					}
					ImGui::Text("      Selection: %s", TCHAR_TO_ANSI(*ModeName));

					if (const FArcRandomPoolSelection_Budget* BudgetMode =
						PoolMod->SelectionMode.GetPtr<FArcRandomPoolSelection_Budget>())
					{
						float TotalBudget = BudgetMode->BaseBudget +
							FMath::Max(0.0f, BandEligQ - 1.0f) * BudgetMode->BudgetPerQuality;
						ImGui::Text("      Budget: %.1f (base: %.1f + %.1f/quality)",
							TotalBudget, BudgetMode->BaseBudget, BudgetMode->BudgetPerQuality);
					}
					else if (const FArcRandomPoolSelection_SimpleRandom* SimpleMode =
						PoolMod->SelectionMode.GetPtr<FArcRandomPoolSelection_SimpleRandom>())
					{
						if (SimpleMode->bQualityAffectsSelections)
						{
							ImGui::Text("      MaxSelections: %d (+bonus every %.1f quality)",
								SimpleMode->MaxSelections, SimpleMode->QualityBonusThreshold);
						}
						else
						{
							ImGui::Text("      MaxSelections: %d", SimpleMode->MaxSelections);
						}
					}
				}

				// List entries with eligibility
				ImGui::Indent();
				for (int32 EntryIdx = 0; EntryIdx < PoolDef->Entries.Num(); ++EntryIdx)
				{
					const FArcRandomPoolEntry& Entry = PoolDef->Entries[EntryIdx];

					bool bEligible = true;
					FString FailReason;

					if (Entry.MinQualityThreshold > 0.0f && BandEligQ < Entry.MinQualityThreshold)
					{
						bEligible = false;
						FailReason = FString::Printf(TEXT("Quality < %.2f"), Entry.MinQualityThreshold);
					}

					FString EntryName = Entry.DisplayName.IsEmpty()
						? FString::Printf(TEXT("Entry %d"), EntryIdx)
						: Entry.DisplayName.ToString();

					if (bEligible)
					{
						float EntryWeight = Entry.BaseWeight;
						EntryWeight *= (1.0f + (BandEligQ - 1.0f) * Entry.QualityWeightScaling);
						EntryWeight = FMath::Max(EntryWeight, 0.0f);

						ImGui::Text("[%d] %s  W:%.2f  Cost:%.1f",
							EntryIdx, TCHAR_TO_ANSI(*EntryName), EntryWeight, Entry.Cost);
					}
					else
					{
						ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
							"[%d] %s  [INELIGIBLE: %s]",
							EntryIdx, TCHAR_TO_ANSI(*EntryName), TCHAR_TO_ANSI(*FailReason));
					}

					if (!Entry.RequiredIngredientTags.IsEmpty())
					{
						ImGui::Text("    RequiredTags: %s", TCHAR_TO_ANSI(*Entry.RequiredIngredientTags.ToStringSimple()));
					}
					if (!Entry.DenyIngredientTags.IsEmpty())
					{
						ImGui::Text("    DenyTags: %s", TCHAR_TO_ANSI(*Entry.DenyIngredientTags.ToStringSimple()));
					}

					if (Entry.Modifiers.Num() > 0)
					{
						int32 StatsCount = 0, AbilCount = 0, EffCount = 0;
						for (const FInstancedStruct& SubMod : Entry.Modifiers)
						{
							if (SubMod.GetPtr<FArcCraftModifier_Stats>()) StatsCount++;
							else if (SubMod.GetPtr<FArcCraftModifier_Abilities>()) AbilCount++;
							else if (SubMod.GetPtr<FArcCraftModifier_Effects>()) EffCount++;
						}
						TArray<FString> Parts;
						if (StatsCount > 0) Parts.Add(FString::Printf(TEXT("%d Stats"), StatsCount));
						if (AbilCount > 0) Parts.Add(FString::Printf(TEXT("%d Abil"), AbilCount));
						if (EffCount > 0) Parts.Add(FString::Printf(TEXT("%d Eff"), EffCount));
						FString Summary = Parts.Num() > 0 ? FString::Join(Parts, TEXT(", ")) : TEXT("(none)");
						ImGui::Text("    Modifiers: %s", TCHAR_TO_ANSI(*Summary));
					}
				}
				ImGui::Unindent();
			}
			else
			{
				ImGui::Text("      Pool: (not set or failed to load)");
			}
		}
	}
}

void FArcCraftImGuiDebugger::DrawOutputPreview()
{
	ImGui::SeparatorText("Output Preview");

	if (CachedEvaluations.Num() == 0)
	{
		ImGui::TextDisabled("No evaluations to preview.");
		return;
	}

	ImGui::Text("Active evaluations: %d", CachedEvaluations.Num());

	for (int32 i = 0; i < CachedEvaluations.Num(); ++i)
	{
		const FArcMaterialRuleEvaluation& Eval = CachedEvaluations[i];
		if (!Eval.Band)
		{
			continue;
		}

		FString RuleName = Eval.Rule ?
			(Eval.Rule->RuleName.IsEmpty() ? FString::Printf(TEXT("Rule %d"), Eval.RuleIndex) : Eval.Rule->RuleName.ToString())
			: TEXT("Unknown");

		FString BandName = Eval.Band->BandName.IsEmpty()
			? FString::Printf(TEXT("Band %d"), Eval.SelectedBandIndex)
			: Eval.Band->BandName.ToString();

		ImGui::PushID(i);
		if (ImGui::TreeNodeEx(
			TCHAR_TO_ANSI(*FString::Printf(TEXT("%s -> %s  (EffWeight: %.3f)"), *RuleName, *BandName, Eval.EffectiveWeight)),
			ImGuiTreeNodeFlags_DefaultOpen))
		{
			DrawBandModifierDetails(*Eval.Band, Eval.BandEligibilityQuality);
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
}

void FArcCraftImGuiDebugger::DrawPendingModifiersTable()
{
	ImGui::SeparatorText("Pending Modifiers");

	if (CachedPendingModifiers.Num() == 0)
	{
		ImGui::TextDisabled("No pending modifiers produced.");
		return;
	}

	ImGui::Text("Total: %d", CachedPendingModifiers.Num());

	if (ImGui::BeginTable("PendingMods", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
	{
		ImGui::TableSetupColumn("#", 0, 30.f);
		ImGui::TableSetupColumn("SlotTag", 0, 120.f);
		ImGui::TableSetupColumn("Weight", 0, 70.f);
		ImGui::TableSetupColumn("Type", 0, 60.f);
		ImGui::TableSetupColumn("Result", 0, 200.f);
		ImGui::TableHeadersRow();

		for (int32 i = 0; i < CachedPendingModifiers.Num(); ++i)
		{
			const FArcCraftPendingModifier& Pending = CachedPendingModifiers[i];

			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%d", i);

			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%s", Pending.SlotTag.IsValid()
				? TCHAR_TO_ANSI(*Pending.SlotTag.ToString())
				: "(unslotted)");

			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%.3f", Pending.EffectiveWeight);

			ImGui::TableSetColumnIndex(3);
			switch (Pending.Result.Type)
			{
			case EArcCraftModifierResultType::Stat:    ImGui::Text("Stat");    break;
			case EArcCraftModifierResultType::Ability: ImGui::Text("Ability"); break;
			case EArcCraftModifierResultType::Effect:  ImGui::Text("Effect");  break;
			default:                                   ImGui::Text("None");    break;
			}

			ImGui::TableSetColumnIndex(4);
			switch (Pending.Result.Type)
			{
			case EArcCraftModifierResultType::Stat:
				{
					FString AttrName = Pending.Result.Stat.Attribute.IsValid()
						? Pending.Result.Stat.Attribute.GetName()
						: TEXT("(none)");
					ImGui::Text("%s: %.2f", TCHAR_TO_ANSI(*AttrName), Pending.Result.Stat.Value.GetValue());
				}
				break;
			case EArcCraftModifierResultType::Ability:
				ImGui::Text("%s", TCHAR_TO_ANSI(*GetNameSafe(Pending.Result.Ability.GrantedAbility)));
				break;
			case EArcCraftModifierResultType::Effect:
				ImGui::Text("%s", TCHAR_TO_ANSI(*GetNameSafe(Pending.Result.Effect.Get())));
				break;
			default:
				ImGui::Text("-");
				break;
			}
		}

		ImGui::EndTable();
	}
}

void FArcCraftImGuiDebugger::RunMonteCarloSimulation(
	const UArcMaterialPropertyTable* Table,
	const FArcMaterialCraftContext& Context)
{
	SimulationResults.Reset();

	if (!Table || Table->Rules.Num() == 0)
	{
		return;
	}

	TMap<FString, int32> ResultMap;

	for (int32 Iter = 0; Iter < SimulationIterations; ++Iter)
	{
		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Context);

		for (const FArcMaterialRuleEvaluation& Eval : Evals)
		{
			if (!Eval.Band || !Eval.Rule)
			{
				continue;
			}

			FString Key = FString::Printf(TEXT("%d_%d"), Eval.RuleIndex, Eval.SelectedBandIndex);
			int32* ExistingIdx = ResultMap.Find(Key);

			if (ExistingIdx)
			{
				FArcCraftImGuiSimBandResult& Result = SimulationResults[*ExistingIdx];
				Result.HitCount++;

				for (const FInstancedStruct& ModStruct : Eval.Band->Modifiers)
				{
					if (const FArcCraftModifier_Stats* StatsMod = ModStruct.GetPtr<FArcCraftModifier_Stats>())
					{
						const float QualityScale = 1.0f + (Eval.BandEligibilityQuality - 1.0f) * StatsMod->QualityScalingFactor;
						FString AttrName = StatsMod->BaseStat.Attribute.IsValid() ? StatsMod->BaseStat.Attribute.GetName() : TEXT("(none)");
						float& Accumulated = Result.AccumulatedStatValues.FindOrAdd(AttrName);
						Accumulated += StatsMod->BaseStat.Value.GetValue() * QualityScale;
					}
				}
			}
			else
			{
				FArcCraftImGuiSimBandResult NewResult;
				NewResult.RuleIndex = Eval.RuleIndex;
				NewResult.BandIndex = Eval.SelectedBandIndex;
				NewResult.HitCount = 1;

				NewResult.RuleName = Eval.Rule->RuleName.IsEmpty()
					? FString::Printf(TEXT("Rule %d"), Eval.RuleIndex)
					: Eval.Rule->RuleName.ToString();

				NewResult.BandName = Eval.Band->BandName.IsEmpty()
					? FString::Printf(TEXT("Band %d"), Eval.SelectedBandIndex)
					: Eval.Band->BandName.ToString();

				int32 StatsCount = 0, AbilCount = 0, EffCount = 0;
				for (const FInstancedStruct& ModStruct : Eval.Band->Modifiers)
				{
					if (ModStruct.GetPtr<FArcCraftModifier_Stats>()) StatsCount++;
					else if (ModStruct.GetPtr<FArcCraftModifier_Abilities>()) AbilCount++;
					else if (ModStruct.GetPtr<FArcCraftModifier_Effects>()) EffCount++;
				}
				TArray<FString> Parts;
				if (StatsCount > 0) Parts.Add(FString::Printf(TEXT("%d Stats"), StatsCount));
				if (AbilCount > 0) Parts.Add(FString::Printf(TEXT("%d Abil"), AbilCount));
				if (EffCount > 0) Parts.Add(FString::Printf(TEXT("%d Eff"), EffCount));
				NewResult.ModifierSummary = Parts.Num() > 0 ? FString::Join(Parts, TEXT(", ")) : TEXT("(none)");

				for (const FInstancedStruct& ModStruct : Eval.Band->Modifiers)
				{
					if (const FArcCraftModifier_Stats* StatsMod = ModStruct.GetPtr<FArcCraftModifier_Stats>())
					{
						const float QualityScale = 1.0f + (Eval.BandEligibilityQuality - 1.0f) * StatsMod->QualityScalingFactor;
						FString AttrName = StatsMod->BaseStat.Attribute.IsValid() ? StatsMod->BaseStat.Attribute.GetName() : TEXT("(none)");
						float& Accumulated = NewResult.AccumulatedStatValues.FindOrAdd(AttrName);
						Accumulated += StatsMod->BaseStat.Value.GetValue() * QualityScale;
					}
				}

				int32 NewIdx = SimulationResults.Add(MoveTemp(NewResult));
				ResultMap.Add(Key, NewIdx);
			}
		}
	}

	// Sort by rule index, then by hit count descending
	SimulationResults.Sort([](const FArcCraftImGuiSimBandResult& A, const FArcCraftImGuiSimBandResult& B)
	{
		if (A.RuleIndex != B.RuleIndex)
		{
			return A.RuleIndex < B.RuleIndex;
		}
		return A.HitCount > B.HitCount;
	});

	bSimulationDirty = false;
}

void FArcCraftImGuiDebugger::DrawSimulationPanel(
	const UArcMaterialPropertyTable* Table,
	const FArcMaterialCraftContext& Context)
{
	ImGui::SeparatorText("Monte Carlo Simulation");

	if (!Table)
	{
		ImGui::TextDisabled("(no table selected)");
		return;
	}

	// Iteration count slider
	ImGui::Text("Iterations:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(160.f);
	ImGui::SliderInt("##SimIterations", &SimulationIterations, 100, 5000);

	if (ImGui::Button("Simulate"))
	{
		RunMonteCarloSimulation(Table, Context);
	}

	if (SimulationResults.Num() == 0)
	{
		ImGui::TextDisabled("Click 'Simulate' to run.");
		return;
	}

	ImGui::Spacing();
	ImGui::Text("Results (%d unique rule+band combinations):", SimulationResults.Num());

	if (ImGui::BeginTable("SimResults", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
	{
		ImGui::TableSetupColumn("Rule", 0, 100.f);
		ImGui::TableSetupColumn("Band", 0, 90.f);
		ImGui::TableSetupColumn("Modifiers", 0, 90.f);
		ImGui::TableSetupColumn("Prob %", 0, 100.f);
		ImGui::TableSetupColumn("Avg Stats", 0, 150.f);
		ImGui::TableHeadersRow();

		for (const FArcCraftImGuiSimBandResult& Result : SimulationResults)
		{
			float Probability = (static_cast<float>(Result.HitCount) / static_cast<float>(SimulationIterations)) * 100.0f;

			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%s", TCHAR_TO_ANSI(*Result.RuleName));

			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%s", TCHAR_TO_ANSI(*Result.BandName));

			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%s", TCHAR_TO_ANSI(*Result.ModifierSummary));

			ImGui::TableSetColumnIndex(3);
			ImGui::ProgressBar(Probability / 100.0f, ImVec2(-FLT_MIN, 0.f),
				TCHAR_TO_ANSI(*FString::Printf(TEXT("%.1f%%"), Probability)));

			ImGui::TableSetColumnIndex(4);
			if (Result.AccumulatedStatValues.Num() > 0)
			{
				for (const auto& Pair : Result.AccumulatedStatValues)
				{
					float AvgVal = Pair.Value / static_cast<float>(Result.HitCount);
					ImGui::Text("%s: %.2f", TCHAR_TO_ANSI(*Pair.Key), AvgVal);
				}
			}
			else
			{
				ImGui::Text("-");
			}
		}

		ImGui::EndTable();
	}
}
