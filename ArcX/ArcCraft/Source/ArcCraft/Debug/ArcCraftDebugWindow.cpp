// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCraftDebugWindow.h"

#include "SlateIM.h"
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
#include "Styling/AppStyle.h"

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
#include "Items/ArcItemTypes.h"
#include "GameplayEffect.h"
#include "Abilities/GameplayAbility.h"

// -------------------------------------------------------------------
// Constructor
// -------------------------------------------------------------------

FArcCraftDebugWindow::FArcCraftDebugWindow()
	: FSlateIMWindowBase(
		TEXT("Arc Craft Debug"),
		FVector2f(1400.f, 700.f),
		TEXT("Arc.Craft.Debug"),
		TEXT("Toggle the Arc Craft debug window"))
{
}

// -------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------

void FArcCraftDebugWindow::RefreshStations()
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

void FArcCraftDebugWindow::RefreshRecipeDefinitions()
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

	// Reset filter
	FilteredRecipeNames = CachedRecipeNames;
	FilteredRecipeToSourceIndex.Reset();
	for (int32 i = 0; i < CachedRecipeNames.Num(); ++i)
	{
		FilteredRecipeToSourceIndex.Add(i);
	}
}

void FArcCraftDebugWindow::RefreshTransferItemDefinitions()
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

	FilteredTransferNames = TransferItemNames;
	FilteredTransferToSourceIndex.Reset();
	for (int32 i = 0; i < TransferItemNames.Num(); ++i)
	{
		FilteredTransferToSourceIndex.Add(i);
	}
}

void FArcCraftDebugWindow::RefreshMaterialRecipes()
{
	CachedMaterialRecipes.Reset();
	CachedMaterialRecipeNames.Reset();
	bMaterialRecipesCached = true;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AllRecipes;
	AssetRegistry.GetAssetsByClass(UArcRecipeDefinition::StaticClass()->GetClassPathName(), AllRecipes, true);

	// Filter to only recipes that have FArcRecipeOutputModifier_MaterialProperties
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

	// Reset filter
	FilteredMaterialRecipeNames = CachedMaterialRecipeNames;
	FilteredMaterialRecipeToSourceIndex.Reset();
	for (int32 i = 0; i < CachedMaterialRecipeNames.Num(); ++i)
	{
		FilteredMaterialRecipeToSourceIndex.Add(i);
	}
}

UArcItemsStoreComponent* FArcCraftDebugWindow::GetLocalPlayerItemStore() const
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

	// Check pawn, then controller, then player state
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

APlayerController* FArcCraftDebugWindow::GetLocalPlayerController() const
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

float FArcCraftDebugWindow::ComputeBandWeight(
	float BaseWeight, float MinQuality, float QualityWeightBias,
	float BandEligibilityQuality, float BandWeightBonus)
{
	return BaseWeight * (1.0f + (FMath::Max(0.0f, BandEligibilityQuality - MinQuality) + BandWeightBonus) * QualityWeightBias);
}

// -------------------------------------------------------------------
// DrawWindow
// -------------------------------------------------------------------

void FArcCraftDebugWindow::DrawWindow(float DeltaTime)
{
	SlateIM::Fill();
	SlateIM::HAlign(HAlign_Fill);
	SlateIM::VAlign(VAlign_Fill);
	SlateIM::BeginTabGroup(TEXT("ArcCraftDebugTabs"));
	SlateIM::BeginTabStack();

	if (SlateIM::BeginTab(TEXT("Stations")))
	{
		SlateIM::BeginBorder(FAppStyle::GetBrush("ToolPanel.GroupBorder"));
		SlateIM::Fill();
		SlateIM::HAlign(HAlign_Fill);
		SlateIM::VAlign(VAlign_Fill);
		DrawStationsTab();
		SlateIM::EndBorder();
	}
	SlateIM::EndTab();

	if (SlateIM::BeginTab(TEXT("Recipes")))
	{
		SlateIM::BeginBorder(FAppStyle::GetBrush("ToolPanel.GroupBorder"));
		SlateIM::Fill();
		SlateIM::HAlign(HAlign_Fill);
		SlateIM::VAlign(VAlign_Fill);
		DrawRecipesTab();
		SlateIM::EndBorder();
	}
	SlateIM::EndTab();

	if (SlateIM::BeginTab(TEXT("Material Preview")))
	{
		SlateIM::BeginBorder(FAppStyle::GetBrush("ToolPanel.GroupBorder"));
		SlateIM::Fill();
		SlateIM::HAlign(HAlign_Fill);
		SlateIM::VAlign(VAlign_Fill);
		DrawMaterialPreviewTab();
		SlateIM::EndBorder();
	}
	SlateIM::EndTab();

	SlateIM::EndTabStack();
	SlateIM::EndTabGroup();
}

// ===================================================================
// Tab: Stations
// ===================================================================

void FArcCraftDebugWindow::DrawStationsTab()
{
	SlateIM::BeginVerticalStack();
	{
		// Toolbar
		SlateIM::Padding(FMargin(4.f));
		SlateIM::BeginHorizontalStack();
		{
			if (SlateIM::Button(TEXT("Refresh Stations")))
			{
				RefreshStations();
				SelectedStationIndex = -1;
				bStationRecipesCached = false;
			}

			SlateIM::Padding(FMargin(8.f, 0.f));
			SlateIM::CheckBox(TEXT("Bypass Ingredients"), bBypassIngredients);

			SlateIM::Padding(FMargin(8.f, 0.f));
			SlateIM::CheckBox(TEXT("Instant Craft"), bInstantCraft);
		}
		SlateIM::EndHorizontalStack();

		// Main content: left list, right detail
		SlateIM::Padding(FMargin(4.f));
		SlateIM::Fill();
		SlateIM::BeginHorizontalStack();
		{
			// Left panel: station list
			SlateIM::MinWidth(300.f);
			SlateIM::VAlign(VAlign_Fill);
			SlateIM::BeginVerticalStack();
			{
				DrawStationList();
			}
			SlateIM::EndVerticalStack();

			// Right panel: station detail
			SlateIM::Fill();
			SlateIM::HAlign(HAlign_Fill);
			SlateIM::VAlign(VAlign_Fill);
			SlateIM::BeginVerticalStack();
			{
				DrawStationDetail();
			}
			SlateIM::EndVerticalStack();
		}
		SlateIM::EndHorizontalStack();
	}
	SlateIM::EndVerticalStack();
}

void FArcCraftDebugWindow::DrawStationList()
{
	if (CachedStations.Num() == 0)
	{
		SlateIM::Padding(FMargin(8.f));
		SlateIM::Text(TEXT("No stations found. Click 'Refresh Stations'."));
		return;
	}

	SlateIM::Fill();
	SlateIM::FTableParams TableParams;
	TableParams.SelectionMode = ESelectionMode::Single;
	SlateIM::BeginTable(TableParams);
	{
		SlateIM::BeginTableHeader();
		{
			SlateIM::InitialTableColumnWidth(180.f);
			SlateIM::AddTableColumn(TEXT("Owner"), TEXT("Owner"));
			SlateIM::InitialTableColumnWidth(60.f);
			SlateIM::AddTableColumn(TEXT("Queue"), TEXT("Queue"));
		}
		SlateIM::EndTableHeader();

		SlateIM::BeginTableBody();
		{
			for (int32 i = 0; i < CachedStations.Num(); ++i)
			{
				if (!CachedStations[i].IsValid())
				{
					continue;
				}

				UArcCraftStationComponent* Station = CachedStations[i].Get();
				bool bRowSelected = false;

				if (SlateIM::NextTableCell(&bRowSelected))
				{
					FString OwnerName = GetNameSafe(Station->GetOwner());
					SlateIM::Text(*OwnerName);
				}

				if (bRowSelected && SelectedStationIndex != i)
				{
					SelectedStationIndex = i;
					bStationRecipesCached = false;
					bShowTransferPanel = false;
				}

				if (SlateIM::NextTableCell())
				{
					SlateIM::Text(*FString::Printf(TEXT("%d"), Station->GetQueue().Num()));
				}
			}
		}
		SlateIM::EndTableBody();
	}
	SlateIM::EndTable();
}

void FArcCraftDebugWindow::DrawStationDetail()
{
	if (SelectedStationIndex < 0 || SelectedStationIndex >= CachedStations.Num() ||
		!CachedStations[SelectedStationIndex].IsValid())
	{
		SlateIM::Padding(FMargin(8.f));
		SlateIM::Text(TEXT("Select a station from the list"));
		return;
	}

	UArcCraftStationComponent* Station = CachedStations[SelectedStationIndex].Get();

	SlateIM::Padding(FMargin(4.f));
	SlateIM::BeginScrollBox();
	{
		// Station info
		SlateIM::Text(*FString::Printf(TEXT("Station: %s"), *GetNameSafe(Station)));
		SlateIM::Text(*FString::Printf(TEXT("Owner: %s"), *GetNameSafe(Station->GetOwner())));
		SlateIM::Text(*FString::Printf(TEXT("Station Tags: %s"), *Station->GetStationTags().ToString()));

		SlateIM::Spacer(FVector2D(0.0, 8.0));

		// Craft queue
		DrawCraftQueue(Station);

		SlateIM::Spacer(FVector2D(0.0, 8.0));

		// Available recipes
		DrawAvailableRecipes(Station);

		SlateIM::Spacer(FVector2D(0.0, 8.0));

		// Transfer items panel
		DrawTransferItemsPanel(Station);

		SlateIM::Spacer(FVector2D(0.0, 8.0));

		// Craftable discovery
		DrawCraftableDiscovery(Station);
	}
	SlateIM::EndScrollBox();
}

void FArcCraftDebugWindow::DrawCraftQueue(UArcCraftStationComponent* Station)
{
	SlateIM::Text(TEXT("--- Craft Queue ---"));

	const TArray<FArcCraftQueueEntry>& Queue = Station->GetQueue();

	if (Queue.Num() == 0)
	{
		SlateIM::Padding(FMargin(8.f, 2.f));
		SlateIM::Text(TEXT("(empty)"));
		return;
	}

	SlateIM::BeginTable();
	{
		SlateIM::BeginTableHeader();
		{
			SlateIM::InitialTableColumnWidth(150.f);
			SlateIM::AddTableColumn(TEXT("Recipe"), TEXT("Recipe"));
			SlateIM::InitialTableColumnWidth(80.f);
			SlateIM::AddTableColumn(TEXT("Progress"), TEXT("Progress"));
			SlateIM::InitialTableColumnWidth(80.f);
			SlateIM::AddTableColumn(TEXT("Time"), TEXT("Time"));
			SlateIM::FixedTableColumnWidth(80.f);
			SlateIM::AddTableColumn(TEXT("Actions"), TEXT("Actions"));
		}
		SlateIM::EndTableHeader();

		SlateIM::BeginTableBody();
		{
			for (int32 i = 0; i < Queue.Num(); ++i)
			{
				const FArcCraftQueueEntry& Entry = Queue[i];

				// Recipe name
				if (SlateIM::NextTableCell())
				{
					FString RecipeName = Entry.Recipe ? Entry.Recipe->RecipeName.ToString() : TEXT("Unknown");
					SlateIM::Text(*RecipeName);
				}

				// Progress
				if (SlateIM::NextTableCell())
				{
					SlateIM::Text(*FString::Printf(TEXT("%d / %d"), Entry.CompletedAmount, Entry.Amount));
				}

				// Time
				if (SlateIM::NextTableCell())
				{
					if (Entry.Recipe)
					{
						float CraftTime = Entry.Recipe->CraftTime;
						float Progress = (CraftTime > 0.0f) ? FMath::Clamp(Entry.ElapsedTickTime / CraftTime, 0.0f, 1.0f) : 1.0f;
						SlateIM::ProgressBar(Progress);
					}
					else
					{
						SlateIM::Text(TEXT("-"));
					}
				}

				// Actions
				if (SlateIM::NextTableCell())
				{
					if (SlateIM::Button(*FString::Printf(TEXT("Cancel##q%d"), i)))
					{
						Station->CancelQueueEntry(Entry.EntryId);
					}
				}
			}
		}
		SlateIM::EndTableBody();
	}
	SlateIM::EndTable();
}

void FArcCraftDebugWindow::DrawAvailableRecipes(UArcCraftStationComponent* Station)
{
	SlateIM::Text(TEXT("--- Available Recipes ---"));

	if (!bStationRecipesCached)
	{
		CachedStationRecipes = Station->GetAvailableRecipes();
		bStationRecipesCached = true;
	}

	if (CachedStationRecipes.Num() == 0)
	{
		SlateIM::Padding(FMargin(8.f, 2.f));
		SlateIM::Text(TEXT("No recipes available for this station."));
		return;
	}

	SlateIM::BeginTable();
	{
		SlateIM::BeginTableHeader();
		{
			SlateIM::InitialTableColumnWidth(200.f);
			SlateIM::AddTableColumn(TEXT("Recipe"), TEXT("Recipe"));
			SlateIM::InitialTableColumnWidth(60.f);
			SlateIM::AddTableColumn(TEXT("Time"), TEXT("Time"));
			SlateIM::InitialTableColumnWidth(80.f);
			SlateIM::AddTableColumn(TEXT("Ingredients"), TEXT("Ingredients"));
			SlateIM::FixedTableColumnWidth(80.f);
			SlateIM::AddTableColumn(TEXT("Actions"), TEXT("Actions"));
		}
		SlateIM::EndTableHeader();

		SlateIM::BeginTableBody();
		{
			APlayerController* PC = GetLocalPlayerController();

			for (int32 i = 0; i < CachedStationRecipes.Num(); ++i)
			{
				UArcRecipeDefinition* Recipe = CachedStationRecipes[i];
				if (!Recipe)
				{
					continue;
				}

				// Recipe name
				if (SlateIM::NextTableCell())
				{
					SlateIM::Text(*Recipe->RecipeName.ToString());
				}

				// Craft time
				if (SlateIM::NextTableCell())
				{
					SlateIM::Text(*FString::Printf(TEXT("%.1fs"), Recipe->CraftTime));
				}

				// Ingredient count
				if (SlateIM::NextTableCell())
				{
					SlateIM::Text(*FString::Printf(TEXT("%d slots"), Recipe->GetIngredientCount()));
				}

				// Queue button
				if (SlateIM::NextTableCell())
				{
					if (SlateIM::Button(*FString::Printf(TEXT("Queue##r%d"), i)))
					{
						// TODO: bBypassIngredients — needs station-side debug API to skip
						// ingredient validation. Currently QueueRecipe always checks ingredients.
						Station->QueueRecipe(Recipe, PC, 1);

						if (bInstantCraft)
						{
							// Immediately complete the most recently added entry
							Station->TryCompleteOnInteraction(PC);
						}
					}
				}
			}
		}
		SlateIM::EndTableBody();
	}
	SlateIM::EndTable();
}

void FArcCraftDebugWindow::DrawTransferItemsPanel(UArcCraftStationComponent* Station)
{
	SlateIM::BeginHorizontalStack();
	{
		SlateIM::Text(TEXT("--- Transfer Items ---"));
		SlateIM::Padding(FMargin(8.f, 0.f));
		if (SlateIM::Button(bShowTransferPanel ? TEXT("Hide") : TEXT("Show")))
		{
			bShowTransferPanel = !bShowTransferPanel;
			if (bShowTransferPanel && TransferItemDefinitions.Num() == 0)
			{
				RefreshTransferItemDefinitions();
			}
		}
	}
	SlateIM::EndHorizontalStack();

	if (!bShowTransferPanel)
	{
		return;
	}

	SlateIM::Padding(FMargin(4.f));
	SlateIM::BeginBorder(TEXT("ToolPanel.GroupBorder"));
	{
		// Option 1: Transfer from player store
		UArcItemsStoreComponent* PlayerStore = GetLocalPlayerItemStore();
		if (PlayerStore)
		{
			SlateIM::Text(TEXT("Player Store Items:"));

			TArray<const FArcItemData*> Items = PlayerStore->GetItems();
			if (Items.Num() == 0)
			{
				SlateIM::Padding(FMargin(8.f, 2.f));
				SlateIM::Text(TEXT("(no items in player store)"));
			}
			else
			{
				SlateIM::MaxHeight(150.f);
				SlateIM::BeginScrollBox();
				{
					for (int32 i = 0; i < Items.Num(); ++i)
					{
						const FArcItemData* Item = Items[i];
						if (!Item)
						{
							continue;
						}

						SlateIM::Padding(FMargin(2.f));
						SlateIM::BeginHorizontalStack();
						{
							const UArcItemDefinition* Def = Item->GetItemDefinition();
							SlateIM::Text(*FString::Printf(TEXT("%s (L%d x%d)"),
								*GetNameSafe(Def), Item->GetLevel(), Item->GetStacks()));

							SlateIM::Padding(FMargin(4.f, 0.f));
							if (SlateIM::Button(*FString::Printf(TEXT("Deposit##t%d"), i)))
							{
								APlayerController* PC = GetLocalPlayerController();
								FArcItemSpec Spec = FArcItemSpec::NewItem(
									const_cast<UArcItemDefinition*>(Def), Item->GetStacks(), Item->GetLevel());
								Station->DepositItem(Spec, PC);
							}
						}
						SlateIM::EndHorizontalStack();
					}
				}
				SlateIM::EndScrollBox();
			}
		}
		else
		{
			SlateIM::Text(TEXT("No player item store found."));
		}

		SlateIM::Spacer(FVector2D(0.0, 4.0));

		// Option 2: Spawn item from definition list
		SlateIM::Text(TEXT("Spawn from Definition:"));

		SlateIM::Padding(FMargin(2.f));
		SlateIM::BeginHorizontalStack();
		{
			SlateIM::Text(TEXT("Filter: "));
			SlateIM::MinWidth(200.f);
			if (SlateIM::EditableText(TransferFilterText, TEXT("Search definitions...")))
			{
				FilteredTransferNames.Reset();
				FilteredTransferToSourceIndex.Reset();
				FString Lower = TransferFilterText.ToLower();
				for (int32 i = 0; i < TransferItemNames.Num(); ++i)
				{
					if (TransferFilterText.IsEmpty() || TransferItemNames[i].ToLower().Contains(Lower))
					{
						FilteredTransferNames.Add(TransferItemNames[i]);
						FilteredTransferToSourceIndex.Add(i);
					}
				}
				SelectedTransferItemIndex = 0;
			}

			SlateIM::Padding(FMargin(4.f, 0.f));
			if (SlateIM::Button(TEXT("Refresh Defs")))
			{
				RefreshTransferItemDefinitions();
				TransferFilterText.Empty();
			}
		}
		SlateIM::EndHorizontalStack();

		if (FilteredTransferNames.Num() > 0)
		{
			SlateIM::Padding(FMargin(2.f));
			SlateIM::MaxHeight(120.f);
			SlateIM::SelectionList(FilteredTransferNames, SelectedTransferItemIndex);

			SlateIM::Padding(FMargin(2.f));
			if (SlateIM::Button(TEXT("Deposit Selected")))
			{
				if (SelectedTransferItemIndex >= 0 && SelectedTransferItemIndex < FilteredTransferToSourceIndex.Num())
				{
					int32 SourceIdx = FilteredTransferToSourceIndex[SelectedTransferItemIndex];
					if (SourceIdx >= 0 && SourceIdx < TransferItemDefinitions.Num())
					{
						UArcItemDefinition* Def = Cast<UArcItemDefinition>(TransferItemDefinitions[SourceIdx].GetAsset());
						if (Def)
						{
							APlayerController* PC = GetLocalPlayerController();
							FArcItemSpec Spec = FArcItemSpec::NewItem(Def, 1, 1);
							Station->DepositItem(Spec, PC);
						}
					}
				}
			}
		}
	}
	SlateIM::EndBorder();
}

void FArcCraftDebugWindow::DrawCraftableDiscovery(UArcCraftStationComponent* Station)
{
	SlateIM::Text(TEXT("--- Craftable Discovery ---"));
	SlateIM::Padding(FMargin(2.f));
	SlateIM::Text(TEXT("Recipes that can be crafted with currently available ingredients:"));

	APlayerController* PC = GetLocalPlayerController();
	TArray<UArcRecipeDefinition*> Craftable = Station->GetCraftableRecipes(PC);

	if (Craftable.Num() == 0)
	{
		SlateIM::Padding(FMargin(8.f, 2.f));
		SlateIM::Text(TEXT("(none — deposit more items or check station tags)"));
		return;
	}

	for (int32 i = 0; i < Craftable.Num(); ++i)
	{
		UArcRecipeDefinition* Recipe = Craftable[i];
		if (!Recipe)
		{
			continue;
		}

		SlateIM::Padding(FMargin(2.f));
		SlateIM::BeginHorizontalStack();
		{
			SlateIM::Text(*FString::Printf(TEXT("%s (%.1fs, %d ingredients)"),
				*Recipe->RecipeName.ToString(),
				Recipe->CraftTime,
				Recipe->GetIngredientCount()));

			SlateIM::Padding(FMargin(4.f, 0.f));
			if (SlateIM::Button(*FString::Printf(TEXT("Queue##d%d"), i)))
			{
				Station->QueueRecipe(Recipe, PC, 1);
				if (bInstantCraft)
				{
					Station->TryCompleteOnInteraction(PC);
				}
			}
		}
		SlateIM::EndHorizontalStack();
	}
}

// ===================================================================
// Tab: Recipes
// ===================================================================

void FArcCraftDebugWindow::DrawRecipesTab()
{
	SlateIM::BeginVerticalStack();
	{
		SlateIM::Padding(FMargin(4.f));
		SlateIM::Fill();
		SlateIM::BeginHorizontalStack();
		{
			// Left panel: recipe list
			SlateIM::MinWidth(280.f);
			SlateIM::VAlign(VAlign_Fill);
			SlateIM::BeginVerticalStack();
			{
				DrawRecipeList();
			}
			SlateIM::EndVerticalStack();

			// Right panel: recipe detail
			SlateIM::Fill();
			SlateIM::HAlign(HAlign_Fill);
			SlateIM::VAlign(VAlign_Fill);
			SlateIM::BeginVerticalStack();
			{
				if (SelectedRecipeIndex >= 0 && SelectedRecipeIndex < FilteredRecipeToSourceIndex.Num())
				{
					int32 SourceIdx = FilteredRecipeToSourceIndex[SelectedRecipeIndex];
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
					SlateIM::Padding(FMargin(8.f));
					SlateIM::Text(TEXT("Select a recipe to view details"));
				}
			}
			SlateIM::EndVerticalStack();
		}
		SlateIM::EndHorizontalStack();
	}
	SlateIM::EndVerticalStack();
}

void FArcCraftDebugWindow::DrawRecipeList()
{
	SlateIM::Padding(FMargin(4.f));
	SlateIM::BeginHorizontalStack();
	{
		if (SlateIM::Button(TEXT("Refresh")))
		{
			RefreshRecipeDefinitions();
			SelectedRecipeIndex = -1;
		}
	}
	SlateIM::EndHorizontalStack();

	// Filter
	SlateIM::Padding(FMargin(4.f));
	SlateIM::BeginHorizontalStack();
	{
		SlateIM::Text(TEXT("Filter: "));
		SlateIM::MinWidth(180.f);
		if (SlateIM::EditableText(RecipeFilterText, TEXT("Search recipes...")))
		{
			FilteredRecipeNames.Reset();
			FilteredRecipeToSourceIndex.Reset();
			FString Lower = RecipeFilterText.ToLower();
			for (int32 i = 0; i < CachedRecipeNames.Num(); ++i)
			{
				if (RecipeFilterText.IsEmpty() || CachedRecipeNames[i].ToLower().Contains(Lower))
				{
					FilteredRecipeNames.Add(CachedRecipeNames[i]);
					FilteredRecipeToSourceIndex.Add(i);
				}
			}
			SelectedRecipeIndex = -1;
		}
	}
	SlateIM::EndHorizontalStack();

	if (CachedRecipeNames.Num() == 0)
	{
		SlateIM::Padding(FMargin(8.f));
		SlateIM::Text(TEXT("Click 'Refresh' to load recipe definitions."));
		return;
	}

	SlateIM::Padding(FMargin(4.f));
	SlateIM::Fill();
	SlateIM::SelectionList(FilteredRecipeNames, SelectedRecipeIndex);
}

void FArcCraftDebugWindow::DrawRecipeDetail(UArcRecipeDefinition* Recipe)
{
	if (!Recipe)
	{
		return;
	}

	SlateIM::Padding(FMargin(4.f));
	SlateIM::BeginScrollBox();
	{
		// Basic info
		SlateIM::Text(*FString::Printf(TEXT("Name: %s"), *Recipe->RecipeName.ToString()));
		SlateIM::Text(*FString::Printf(TEXT("ID: %s"), *Recipe->RecipeId.ToString()));
		SlateIM::Text(*FString::Printf(TEXT("Craft Time: %.1f s"), Recipe->CraftTime));

		if (Recipe->RecipeTags.Num() > 0)
		{
			SlateIM::Text(*FString::Printf(TEXT("Recipe Tags: %s"), *Recipe->RecipeTags.ToString()));
		}

		// Requirements
		SlateIM::Spacer(FVector2D(0.0, 8.0));
		SlateIM::Text(TEXT("--- Requirements ---"));
		if (Recipe->RequiredStationTags.Num() > 0)
		{
			SlateIM::Text(*FString::Printf(TEXT("Station Tags: %s"), *Recipe->RequiredStationTags.ToString()));
		}
		else
		{
			SlateIM::Text(TEXT("Station Tags: (none)"));
		}
		if (Recipe->RequiredInstigatorTags.Num() > 0)
		{
			SlateIM::Text(*FString::Printf(TEXT("Instigator Tags: %s"), *Recipe->RequiredInstigatorTags.ToString()));
		}

		// Ingredients
		SlateIM::Spacer(FVector2D(0.0, 8.0));
		SlateIM::Text(TEXT("--- Ingredients ---"));
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

			// Check for specific types
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

			SlateIM::Text(*IngredientInfo);
		}

		// Output
		SlateIM::Spacer(FVector2D(0.0, 8.0));
		SlateIM::Text(TEXT("--- Output ---"));
		SlateIM::Text(*FString::Printf(TEXT("Item: %s"), *Recipe->OutputItemDefinition.ToString()));
		SlateIM::Text(*FString::Printf(TEXT("Amount: %d"), Recipe->OutputAmount));
		SlateIM::Text(*FString::Printf(TEXT("Level: %d"), Recipe->OutputLevel));

		// Output modifiers
		if (Recipe->OutputModifiers.Num() > 0)
		{
			SlateIM::Spacer(FVector2D(0.0, 8.0));
			SlateIM::Text(TEXT("--- Output Modifiers ---"));
			for (int32 i = 0; i < Recipe->OutputModifiers.Num(); ++i)
			{
				const FInstancedStruct& Modifier = Recipe->OutputModifiers[i];
				if (Modifier.IsValid())
				{
					const UScriptStruct* Struct = Modifier.GetScriptStruct();
					SlateIM::Text(*FString::Printf(TEXT("  [%d] %s"), i, *GetNameSafe(Struct)));
				}
			}
		}

		// Quality
		SlateIM::Spacer(FVector2D(0.0, 8.0));
		SlateIM::Text(TEXT("--- Quality ---"));
		SlateIM::Text(*FString::Printf(TEXT("Quality Affects Level: %s"),
			Recipe->bQualityAffectsLevel ? TEXT("Yes") : TEXT("No")));
		if (!Recipe->QualityTierTable.IsNull())
		{
			SlateIM::Text(*FString::Printf(TEXT("Tier Table: %s"), *Recipe->QualityTierTable.GetAssetName()));
		}
	}
	SlateIM::EndScrollBox();
}

// ===================================================================
// Tab: Material Preview
// ===================================================================

void FArcCraftDebugWindow::DrawMaterialPreviewTab()
{
	SlateIM::BeginVerticalStack();
	{
		SlateIM::Padding(FMargin(4.f));
		SlateIM::Fill();

		// Recipe selector (always visible at top)
		DrawMaterialRecipeSelector();

		// Only show the rest if a recipe with material modifier is selected
		if (SelectedMaterialRecipeIndex >= 0 &&
			SelectedMaterialRecipeIndex < FilteredMaterialRecipeToSourceIndex.Num())
		{
			int32 SourceIdx = FilteredMaterialRecipeToSourceIndex[SelectedMaterialRecipeIndex];
			if (SourceIdx >= 0 && SourceIdx < CachedMaterialRecipes.Num())
			{
				UArcRecipeDefinition* Recipe = Cast<UArcRecipeDefinition>(
					CachedMaterialRecipes[SourceIdx].GetAsset());

				if (Recipe)
				{
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

					if (MatMod)
					{
						UArcMaterialPropertyTable* Table = MatMod->PropertyTable.LoadSynchronous();

						SlateIM::Spacer(FVector2D(0.0, 8.0));
						DrawSimulatedIngredients();

						// Build context and evaluate
						if (bEvaluationDirty && Table)
						{
							// Build simulated context
							TArray<const FArcItemData*> SimIngredients;
							TArray<float> QualityMults;
							float AvgQuality = 1.0f;

							// For simulation, we create placeholder data
							// The quality is either from override or default 1.0
							int32 SlotCount = Recipe->GetIngredientCount();
							for (int32 i = 0; i < SlotCount; ++i)
							{
								SimIngredients.Add(nullptr); // Simplified: no actual item data
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

							FArcMaterialCraftEvaluator::ComputeQualityAndWeightBonus(Table, CachedMaterialContext);

							if (bUseQualityOverride)
							{
								CachedMaterialContext.BandEligibilityQuality = QualityOverride;
							}

							CachedEvaluations = FArcMaterialCraftEvaluator::EvaluateRules(Table, CachedMaterialContext);
							bEvaluationDirty = false;
						}

						SlateIM::Spacer(FVector2D(0.0, 8.0));

						// Quality override slider
						SlateIM::Padding(FMargin(4.f));
						SlateIM::BeginHorizontalStack();
						{
							if (SlateIM::CheckBox(TEXT("Quality Override"), bUseQualityOverride))
							{
								bEvaluationDirty = true;
								bSimulationDirty = true;
							}

							if (bUseQualityOverride)
							{
								SlateIM::Padding(FMargin(8.f, 0.f));
								SlateIM::MinWidth(200.f);
								if (SlateIM::Slider(QualityOverride, 0.0f, 5.0f, 0.1f))
								{
									bEvaluationDirty = true;
									bSimulationDirty = true;
								}
								SlateIM::Padding(FMargin(4.f, 0.f));
								SlateIM::Text(*FString::Printf(TEXT("%.2f"), QualityOverride));
							}
						}
						SlateIM::EndHorizontalStack();

						// Re-roll button
						SlateIM::Padding(FMargin(4.f));
						if (SlateIM::Button(TEXT("Re-roll")))
						{
							bEvaluationDirty = true;
						}

						SlateIM::Spacer(FVector2D(0.0, 8.0));

						// Two-column layout: left = evaluation details, right = simulation
						SlateIM::BeginHorizontalStack();
						{
							// Left column: evaluation details
							SlateIM::MinWidth(580.f);
							SlateIM::Fill();
							SlateIM::HAlign(HAlign_Fill);
							SlateIM::VAlign(VAlign_Fill);
							SlateIM::BeginVerticalStack();
							{
								SlateIM::Fill();
								SlateIM::BeginScrollBox();
								{
									// Context display
									DrawMaterialContextDisplay(CachedMaterialContext);

									// Rule evaluations
									if (Table)
									{
										SlateIM::Spacer(FVector2D(0.0, 8.0));
										DrawRuleEvaluations(Table, CachedMaterialContext);
									}

									// Output preview
									SlateIM::Spacer(FVector2D(0.0, 8.0));
									DrawOutputPreview();
								}
								SlateIM::EndScrollBox();
							}
							SlateIM::EndVerticalStack();

							// Right column: simulation panel
							SlateIM::MinWidth(380.f);
							SlateIM::BeginVerticalStack();
							{
								SlateIM::Fill();
								SlateIM::BeginScrollBox();
								{
									DrawSimulationPanel(Table, CachedMaterialContext);
								}
								SlateIM::EndScrollBox();
							}
							SlateIM::EndVerticalStack();
						}
						SlateIM::EndHorizontalStack();
					}
				}
			}
		}
	}
	SlateIM::EndVerticalStack();
}

void FArcCraftDebugWindow::DrawMaterialRecipeSelector()
{
	SlateIM::Text(TEXT("--- Material Recipe ---"));

	SlateIM::Padding(FMargin(4.f));
	SlateIM::BeginHorizontalStack();
	{
		if (SlateIM::Button(TEXT("Refresh")))
		{
			RefreshMaterialRecipes();
			SelectedMaterialRecipeIndex = -1;
			bEvaluationDirty = true;
			bSimulationDirty = true;
		}

		SlateIM::Padding(FMargin(4.f, 0.f));
		SlateIM::Text(TEXT("Filter: "));
		SlateIM::MinWidth(180.f);
		if (SlateIM::EditableText(MaterialRecipeFilterText, TEXT("Search...")))
		{
			FilteredMaterialRecipeNames.Reset();
			FilteredMaterialRecipeToSourceIndex.Reset();
			FString Lower = MaterialRecipeFilterText.ToLower();
			for (int32 i = 0; i < CachedMaterialRecipeNames.Num(); ++i)
			{
				if (MaterialRecipeFilterText.IsEmpty() || CachedMaterialRecipeNames[i].ToLower().Contains(Lower))
				{
					FilteredMaterialRecipeNames.Add(CachedMaterialRecipeNames[i]);
					FilteredMaterialRecipeToSourceIndex.Add(i);
				}
			}
			SelectedMaterialRecipeIndex = -1;
			bEvaluationDirty = true;
			bSimulationDirty = true;
		}
	}
	SlateIM::EndHorizontalStack();

	if (!bMaterialRecipesCached)
	{
		SlateIM::Padding(FMargin(8.f));
		SlateIM::Text(TEXT("Click 'Refresh' to load recipes with material properties."));
		return;
	}

	if (FilteredMaterialRecipeNames.Num() == 0)
	{
		SlateIM::Padding(FMargin(8.f));
		SlateIM::Text(TEXT("No recipes with Material Properties modifier found."));
		return;
	}

	SlateIM::Padding(FMargin(4.f));
	SlateIM::MaxHeight(120.f);
	int32 PrevIndex = SelectedMaterialRecipeIndex;
	SlateIM::SelectionList(FilteredMaterialRecipeNames, SelectedMaterialRecipeIndex);
	if (SelectedMaterialRecipeIndex != PrevIndex)
	{
		bEvaluationDirty = true;
		bSimulationDirty = true;
		SimulatedIngredientDefIndices.Reset();
	}
}

void FArcCraftDebugWindow::DrawSimulatedIngredients()
{
	SlateIM::Text(TEXT("--- Simulated Ingredients ---"));
	SlateIM::Padding(FMargin(2.f));
	SlateIM::Text(TEXT("(Ingredient simulation simplified — uses per-slot quality via override slider)"));
}

void FArcCraftDebugWindow::DrawMaterialContextDisplay(const FArcMaterialCraftContext& Context)
{
	SlateIM::Text(TEXT("--- Evaluation Context ---"));
	SlateIM::Padding(FMargin(4.f));

	SlateIM::Text(*FString::Printf(TEXT("Average Quality: %.3f"), Context.AverageQuality));
	SlateIM::Text(*FString::Printf(TEXT("Band Eligibility Quality: %.3f"), Context.BandEligibilityQuality));
	SlateIM::Text(*FString::Printf(TEXT("Band Weight Bonus: %.3f"), Context.BandWeightBonus));
	SlateIM::Text(*FString::Printf(TEXT("Ingredient Count: %d (base: %d)"), Context.IngredientCount, Context.BaseIngredientCount));
	SlateIM::Text(*FString::Printf(TEXT("Extra Craft Time Bonus: %.3f"), Context.ExtraCraftTimeBonus));

	if (Context.RecipeTags.Num() > 0)
	{
		SlateIM::Text(*FString::Printf(TEXT("Recipe Tags: %s"), *Context.RecipeTags.ToString()));
	}

	if (Context.PerSlotTags.Num() > 0)
	{
		SlateIM::Text(*FString::Printf(TEXT("Per-Slot Tags: %d slots"), Context.PerSlotTags.Num()));
		for (int32 i = 0; i < Context.PerSlotTags.Num(); ++i)
		{
			if (Context.PerSlotTags[i].Num() > 0)
			{
				SlateIM::Text(*FString::Printf(TEXT("  Slot %d: %s"), i, *Context.PerSlotTags[i].ToString()));
			}
		}
	}
}

void FArcCraftDebugWindow::DrawRuleEvaluations(
	const UArcMaterialPropertyTable* Table,
	const FArcMaterialCraftContext& Context)
{
	SlateIM::Text(TEXT("--- Rule Evaluations ---"));

	if (!Table)
	{
		SlateIM::Padding(FMargin(8.f, 2.f));
		SlateIM::Text(TEXT("(no property table)"));
		return;
	}

	SlateIM::Padding(FMargin(4.f));
	SlateIM::Text(*FString::Printf(TEXT("Table: %s (%d rules, MaxActive: %d)"),
		*Table->TableName.ToString(),
		Table->Rules.Num(),
		Table->MaxActiveRules));

	if (Table->BaseBandBudget > 0.0f)
	{
		float TotalBudget = Table->BaseBandBudget +
			FMath::Max(0.0f, Context.BandEligibilityQuality - 1.0f) * Table->BudgetPerQuality;
		SlateIM::Text(*FString::Printf(TEXT("Band Budget: %.1f (base: %.1f, per quality: %.1f)"),
			TotalBudget, Table->BaseBandBudget, Table->BudgetPerQuality));
	}

	if (CachedEvaluations.Num() == 0)
	{
		SlateIM::Padding(FMargin(8.f, 2.f));
		SlateIM::Text(TEXT("No rules matched."));
		return;
	}

	SlateIM::Text(*FString::Printf(TEXT("Matched Rules: %d"), CachedEvaluations.Num()));
	SlateIM::Spacer(FVector2D(0.0, 4.0));

	for (int32 EvalIdx = 0; EvalIdx < CachedEvaluations.Num(); ++EvalIdx)
	{
		const FArcMaterialRuleEvaluation& Eval = CachedEvaluations[EvalIdx];
		if (!Eval.Rule)
		{
			continue;
		}

		const FArcMaterialPropertyRule& Rule = *Eval.Rule;
		FString RuleName = Rule.RuleName.IsEmpty() ? FString::Printf(TEXT("Rule %d"), Eval.RuleIndex) : Rule.RuleName.ToString();

		SlateIM::Padding(FMargin(4.f));
		SlateIM::BeginBorder(TEXT("ToolPanel.GroupBorder"));
		{
			SlateIM::Text(*FString::Printf(TEXT("Rule: %s (Priority: %d)"), *RuleName, Rule.Priority));

			if (Eval.Band)
			{
				FString BandName = Eval.Band->BandName.IsEmpty()
					? FString::Printf(TEXT("Band %d"), Eval.SelectedBandIndex)
					: Eval.Band->BandName.ToString();
				SlateIM::Text(*FString::Printf(TEXT("Selected Band: %s (index %d)"),
					*BandName, Eval.SelectedBandIndex));
				SlateIM::Text(*FString::Printf(TEXT("Effective Weight: %.3f"), Eval.EffectiveWeight));
			}

			if (Rule.OutputTags.Num() > 0)
			{
				SlateIM::Text(*FString::Printf(TEXT("Output Tags: %s"), *Rule.OutputTags.ToString()));
			}

			// Band probabilities for this rule
			DrawBandProbabilities(Rule, Context.BandEligibilityQuality, Context.BandWeightBonus);
		}
		SlateIM::EndBorder();
	}

	// Also show non-matched rules for reference
	SlateIM::Spacer(FVector2D(0.0, 8.0));
	SlateIM::Text(TEXT("All Rules in Table:"));

	for (int32 RuleIdx = 0; RuleIdx < Table->Rules.Num(); ++RuleIdx)
	{
		const FArcMaterialPropertyRule& Rule = Table->Rules[RuleIdx];
		FString RuleName = Rule.RuleName.IsEmpty()
			? FString::Printf(TEXT("Rule %d"), RuleIdx)
			: Rule.RuleName.ToString();

		// Check if this rule was matched
		bool bMatched = false;
		for (const FArcMaterialRuleEvaluation& Eval : CachedEvaluations)
		{
			if (Eval.RuleIndex == RuleIdx)
			{
				bMatched = true;
				break;
			}
		}

		SlateIM::Padding(FMargin(2.f));
		SlateIM::Text(*FString::Printf(TEXT("  [%d] %s (Priority: %d) %s"),
			RuleIdx, *RuleName, Rule.Priority,
			bMatched ? TEXT("[MATCHED]") : TEXT("[not matched]")));
	}
}

void FArcCraftDebugWindow::DrawBandProbabilities(
	const FArcMaterialPropertyRule& Rule,
	float BandEligQ,
	float WeightBonus)
{
	const TArray<FArcMaterialQualityBand>& Bands = Rule.GetEffectiveQualityBands();

	if (Bands.Num() == 0)
	{
		SlateIM::Padding(FMargin(8.f, 2.f));
		SlateIM::Text(TEXT("(no quality bands)"));
		return;
	}

	// Compute eligible bands and weights inline (GetEligibleBands is private)
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

	SlateIM::Padding(FMargin(4.f));
	SlateIM::Text(*FString::Printf(TEXT("Band Probabilities (Eligible: %d / %d):"),
		EligibleIndices.Num(), Bands.Num()));

	SlateIM::BeginTable();
	{
		SlateIM::BeginTableHeader();
		{
			SlateIM::InitialTableColumnWidth(120.f);
			SlateIM::AddTableColumn(TEXT("Band"), TEXT("Band"));
			SlateIM::InitialTableColumnWidth(60.f);
			SlateIM::AddTableColumn(TEXT("MinQ"), TEXT("MinQ"));
			SlateIM::InitialTableColumnWidth(60.f);
			SlateIM::AddTableColumn(TEXT("BaseW"), TEXT("BaseW"));
			SlateIM::InitialTableColumnWidth(60.f);
			SlateIM::AddTableColumn(TEXT("Bias"), TEXT("Bias"));
			SlateIM::InitialTableColumnWidth(80.f);
			SlateIM::AddTableColumn(TEXT("EffWeight"), TEXT("EffWeight"));
			SlateIM::InitialTableColumnWidth(80.f);
			SlateIM::AddTableColumn(TEXT("Prob %"), TEXT("Prob %"));
		}
		SlateIM::EndTableHeader();

		SlateIM::BeginTableBody();
		{
			for (int32 EligIdx = 0; EligIdx < EligibleIndices.Num(); ++EligIdx)
			{
				int32 BandIdx = EligibleIndices[EligIdx];
				const FArcMaterialQualityBand& Band = Bands[BandIdx];
				float EffWeight = Weights[EligIdx];
				float Probability = (TotalWeight > 0.0f) ? (EffWeight / TotalWeight * 100.0f) : 0.0f;

				// Band name
				if (SlateIM::NextTableCell())
				{
					FString BandName = Band.BandName.IsEmpty()
						? FString::Printf(TEXT("Band %d"), BandIdx)
						: Band.BandName.ToString();
					SlateIM::Text(*BandName);
				}

				// MinQuality
				if (SlateIM::NextTableCell())
				{
					SlateIM::Text(*FString::Printf(TEXT("%.2f"), Band.MinQuality));
				}

				// BaseWeight
				if (SlateIM::NextTableCell())
				{
					SlateIM::Text(*FString::Printf(TEXT("%.2f"), Band.BaseWeight));
				}

				// QualityWeightBias
				if (SlateIM::NextTableCell())
				{
					SlateIM::Text(*FString::Printf(TEXT("%.2f"), Band.QualityWeightBias));
				}

				// Effective weight
				if (SlateIM::NextTableCell())
				{
					SlateIM::Text(*FString::Printf(TEXT("%.3f"), EffWeight));
				}

				// Probability
				if (SlateIM::NextTableCell())
				{
					SlateIM::ProgressBar(Probability / 100.0f);
					SlateIM::Text(*FString::Printf(TEXT("%.1f%%"), Probability));
				}
			}

			// Show ineligible bands in grey
			for (int32 BandIdx = 0; BandIdx < Bands.Num(); ++BandIdx)
			{
				if (EligibleIndices.Contains(BandIdx))
				{
					continue;
				}

				const FArcMaterialQualityBand& Band = Bands[BandIdx];

				if (SlateIM::NextTableCell())
				{
					FString BandName = Band.BandName.IsEmpty()
						? FString::Printf(TEXT("Band %d"), BandIdx)
						: Band.BandName.ToString();
					SlateIM::Text(*FString::Printf(TEXT("%s (ineligible)"), *BandName));
				}

				if (SlateIM::NextTableCell())
				{
					SlateIM::Text(*FString::Printf(TEXT("%.2f"), Band.MinQuality));
				}

				if (SlateIM::NextTableCell())
				{
					SlateIM::Text(*FString::Printf(TEXT("%.2f"), Band.BaseWeight));
				}

				if (SlateIM::NextTableCell())
				{
					SlateIM::Text(*FString::Printf(TEXT("%.2f"), Band.QualityWeightBias));
				}

				if (SlateIM::NextTableCell())
				{
					SlateIM::Text(TEXT("-"));
				}

				if (SlateIM::NextTableCell())
				{
					SlateIM::Text(TEXT("-"));
				}
			}
		}
		SlateIM::EndTableBody();
	}
	SlateIM::EndTable();

	// Show modifier details for each eligible band
	for (int32 EligIdx = 0; EligIdx < EligibleIndices.Num(); ++EligIdx)
	{
		int32 BandIdx = EligibleIndices[EligIdx];
		const FArcMaterialQualityBand& Band = Bands[BandIdx];
		FString BandName = Band.BandName.IsEmpty()
			? FString::Printf(TEXT("Band %d"), BandIdx)
			: Band.BandName.ToString();

		SlateIM::Padding(FMargin(8.f, 2.f));
		SlateIM::Text(*FString::Printf(TEXT("Modifiers in %s:"), *BandName));
		DrawBandModifierDetails(Band, BandEligQ);
	}
}

void FArcCraftDebugWindow::DrawBandModifierDetails(
	const FArcMaterialQualityBand& Band,
	float BandEligQ)
{
	if (Band.Modifiers.Num() == 0)
	{
		SlateIM::Padding(FMargin(16.f, 2.f));
		SlateIM::Text(TEXT("(no modifiers)"));
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

		SlateIM::Padding(FMargin(16.f, 2.f));

		// Determine type name
		const UScriptStruct* Struct = ModStruct.GetScriptStruct();
		FString TypeName = GetNameSafe(Struct);
		// Clean up struct names for display
		if (TypeName.StartsWith(TEXT("ArcCraftModifier_")))
		{
			TypeName = TypeName.RightChop(17); // "Stats", "Abilities", "Effects"
		}

		// Quality gate check
		const bool bPassesQualityGate = (BaseMod->MinQualityThreshold <= 0.0f) || (BandEligQ >= BaseMod->MinQualityThreshold);
		FString QualityStatus = bPassesQualityGate ? TEXT("PASS") : TEXT("FAIL");

		SlateIM::Text(*FString::Printf(TEXT("[%d] %s  |  QualityGate: %s (min: %.2f)  |  QScale: %.2f  |  Weight: %.2f"),
			ModIdx, *TypeName, *QualityStatus, BaseMod->MinQualityThreshold, BaseMod->QualityScalingFactor, BaseMod->Weight));

		if (!BaseMod->TriggerTags.IsEmpty())
		{
			SlateIM::Padding(FMargin(24.f, 0.f));
			SlateIM::Text(*FString::Printf(TEXT("TriggerTags: %s  (N/A in preview)"), *BaseMod->TriggerTags.ToString()));
		}

		// Type-specific details
		if (const FArcCraftModifier_Stats* StatsMod = ModStruct.GetPtr<FArcCraftModifier_Stats>())
		{
			const float QualityScale = 1.0f + (BandEligQ - 1.0f) * StatsMod->QualityScalingFactor;
			for (const FArcItemAttributeStat& Stat : StatsMod->BaseStats)
			{
				const float BaseVal = Stat.Value.GetValue();
				const float ScaledVal = BaseVal * QualityScale;
				FString AttrName = Stat.Attribute.IsValid() ? Stat.Attribute.GetName() : TEXT("(none)");
				SlateIM::Padding(FMargin(24.f, 0.f));
				SlateIM::Text(*FString::Printf(TEXT("  %s: %.2f -> %.2f (x%.3f)"),
					*AttrName, BaseVal, ScaledVal, QualityScale));
			}
		}
		else if (const FArcCraftModifier_Abilities* AbilMod = ModStruct.GetPtr<FArcCraftModifier_Abilities>())
		{
			for (const FArcAbilityEntry& Entry : AbilMod->AbilitiesToGrant)
			{
				SlateIM::Padding(FMargin(24.f, 0.f));
				SlateIM::Text(*FString::Printf(TEXT("  Ability: %s"),
					*GetNameSafe(Entry.GrantedAbility)));
			}
		}
		else if (const FArcCraftModifier_Effects* EffMod = ModStruct.GetPtr<FArcCraftModifier_Effects>())
		{
			for (const TSubclassOf<UGameplayEffect>& EffClass : EffMod->EffectsToGrant)
			{
				SlateIM::Padding(FMargin(24.f, 0.f));
				SlateIM::Text(*FString::Printf(TEXT("  Effect: %s"),
					*GetNameSafe(EffClass.Get())));
			}
		}
	}
}

void FArcCraftDebugWindow::DrawOutputPreview()
{
	SlateIM::Text(TEXT("--- Output Preview ---"));

	if (CachedEvaluations.Num() == 0)
	{
		SlateIM::Padding(FMargin(8.f, 2.f));
		SlateIM::Text(TEXT("No evaluations to preview."));
		return;
	}

	SlateIM::Padding(FMargin(4.f));
	SlateIM::Text(*FString::Printf(TEXT("Active evaluations: %d"), CachedEvaluations.Num()));

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

		SlateIM::Padding(FMargin(4.f));
		SlateIM::BeginBorder(TEXT("ToolPanel.GroupBorder"));
		{
			SlateIM::Text(*FString::Printf(TEXT("%s -> %s  (EffWeight: %.3f)"),
				*RuleName, *BandName, Eval.EffectiveWeight));
			DrawBandModifierDetails(*Eval.Band, Eval.BandEligibilityQuality);
		}
		SlateIM::EndBorder();
	}
}

void FArcCraftDebugWindow::RunMonteCarloSimulation(
	const UArcMaterialPropertyTable* Table,
	const FArcMaterialCraftContext& Context)
{
	SimulationResults.Reset();

	if (!Table || Table->Rules.Num() == 0)
	{
		return;
	}

	// Map: "RuleIdx_BandIdx" -> index in SimulationResults
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
				FArcCraftSimBandResult& Result = SimulationResults[*ExistingIdx];
				Result.HitCount++;

				// Accumulate stat values
				for (const FInstancedStruct& ModStruct : Eval.Band->Modifiers)
				{
					if (const FArcCraftModifier_Stats* StatsMod = ModStruct.GetPtr<FArcCraftModifier_Stats>())
					{
						const float QualityScale = 1.0f + (Eval.BandEligibilityQuality - 1.0f) * StatsMod->QualityScalingFactor;
						for (const FArcItemAttributeStat& Stat : StatsMod->BaseStats)
						{
							FString AttrName = Stat.Attribute.IsValid() ? Stat.Attribute.GetName() : TEXT("(none)");
							float& Accumulated = Result.AccumulatedStatValues.FindOrAdd(AttrName);
							Accumulated += Stat.Value.GetValue() * QualityScale;
						}
					}
				}
			}
			else
			{
				FArcCraftSimBandResult NewResult;
				NewResult.RuleIndex = Eval.RuleIndex;
				NewResult.BandIndex = Eval.SelectedBandIndex;
				NewResult.HitCount = 1;

				NewResult.RuleName = Eval.Rule->RuleName.IsEmpty()
					? FString::Printf(TEXT("Rule %d"), Eval.RuleIndex)
					: Eval.Rule->RuleName.ToString();

				NewResult.BandName = Eval.Band->BandName.IsEmpty()
					? FString::Printf(TEXT("Band %d"), Eval.SelectedBandIndex)
					: Eval.Band->BandName.ToString();

				// Build modifier summary
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

				// Accumulate initial stat values
				for (const FInstancedStruct& ModStruct : Eval.Band->Modifiers)
				{
					if (const FArcCraftModifier_Stats* StatsMod = ModStruct.GetPtr<FArcCraftModifier_Stats>())
					{
						const float QualityScale = 1.0f + (Eval.BandEligibilityQuality - 1.0f) * StatsMod->QualityScalingFactor;
						for (const FArcItemAttributeStat& Stat : StatsMod->BaseStats)
						{
							FString AttrName = Stat.Attribute.IsValid() ? Stat.Attribute.GetName() : TEXT("(none)");
							float& Accumulated = NewResult.AccumulatedStatValues.FindOrAdd(AttrName);
							Accumulated += Stat.Value.GetValue() * QualityScale;
						}
					}
				}

				int32 NewIdx = SimulationResults.Add(MoveTemp(NewResult));
				ResultMap.Add(Key, NewIdx);
			}
		}
	}

	// Sort by rule index, then by hit count descending
	SimulationResults.Sort([](const FArcCraftSimBandResult& A, const FArcCraftSimBandResult& B)
	{
		if (A.RuleIndex != B.RuleIndex)
		{
			return A.RuleIndex < B.RuleIndex;
		}
		return A.HitCount > B.HitCount;
	});

	bSimulationDirty = false;
}

void FArcCraftDebugWindow::DrawSimulationPanel(
	const UArcMaterialPropertyTable* Table,
	const FArcMaterialCraftContext& Context)
{
	SlateIM::Text(TEXT("--- Monte Carlo Simulation ---"));

	if (!Table)
	{
		SlateIM::Padding(FMargin(8.f, 2.f));
		SlateIM::Text(TEXT("(no table selected)"));
		return;
	}

	// Iteration count slider
	SlateIM::Padding(FMargin(4.f));
	SlateIM::BeginHorizontalStack();
	{
		SlateIM::Text(TEXT("Iterations: "));
		SlateIM::MinWidth(160.f);
		float FloatIter = static_cast<float>(SimulationIterations);
		if (SlateIM::Slider(FloatIter, 100.0f, 5000.0f, 100.0f))
		{
			SimulationIterations = static_cast<int32>(FloatIter);
			bSimulationDirty = true;
		}
		SlateIM::Padding(FMargin(4.f, 0.f));
		SlateIM::Text(*FString::Printf(TEXT("%d"), SimulationIterations));
	}
	SlateIM::EndHorizontalStack();

	// Simulate button
	SlateIM::Padding(FMargin(4.f));
	if (SlateIM::Button(TEXT("Simulate")))
	{
		RunMonteCarloSimulation(Table, Context);
	}

	if (SimulationResults.Num() == 0)
	{
		SlateIM::Padding(FMargin(8.f, 2.f));
		SlateIM::Text(TEXT("Click 'Simulate' to run."));
		return;
	}

	SlateIM::Spacer(FVector2D(0.0, 4.0));
	SlateIM::Text(*FString::Printf(TEXT("Results (%d unique rule+band combinations):"), SimulationResults.Num()));

	// Results table
	SlateIM::BeginTable();
	{
		SlateIM::BeginTableHeader();
		{
			SlateIM::InitialTableColumnWidth(100.f);
			SlateIM::AddTableColumn(TEXT("Rule"), TEXT("Rule"));
			SlateIM::InitialTableColumnWidth(90.f);
			SlateIM::AddTableColumn(TEXT("Band"), TEXT("Band"));
			SlateIM::InitialTableColumnWidth(90.f);
			SlateIM::AddTableColumn(TEXT("Modifiers"), TEXT("Modifiers"));
			SlateIM::InitialTableColumnWidth(80.f);
			SlateIM::AddTableColumn(TEXT("Prob %"), TEXT("Prob %"));
			SlateIM::InitialTableColumnWidth(120.f);
			SlateIM::AddTableColumn(TEXT("Avg Stats"), TEXT("Avg Stats"));
		}
		SlateIM::EndTableHeader();

		SlateIM::BeginTableBody();
		{
			for (const FArcCraftSimBandResult& Result : SimulationResults)
			{
				float Probability = (static_cast<float>(Result.HitCount) / static_cast<float>(SimulationIterations)) * 100.0f;

				// Rule
				if (SlateIM::NextTableCell())
				{
					SlateIM::Text(*Result.RuleName);
				}

				// Band
				if (SlateIM::NextTableCell())
				{
					SlateIM::Text(*Result.BandName);
				}

				// Modifiers
				if (SlateIM::NextTableCell())
				{
					SlateIM::Text(*Result.ModifierSummary);
				}

				// Probability
				if (SlateIM::NextTableCell())
				{
					SlateIM::ProgressBar(Probability / 100.0f);
					SlateIM::Text(*FString::Printf(TEXT("%.1f%%"), Probability));
				}

				// Average stats
				if (SlateIM::NextTableCell())
				{
					if (Result.AccumulatedStatValues.Num() > 0)
					{
						for (const auto& Pair : Result.AccumulatedStatValues)
						{
							float AvgVal = Pair.Value / static_cast<float>(Result.HitCount);
							SlateIM::Text(*FString::Printf(TEXT("%s: %.2f"), *Pair.Key, AvgVal));
						}
					}
					else
					{
						SlateIM::Text(TEXT("-"));
					}
				}
			}
		}
		SlateIM::EndTableBody();
	}
	SlateIM::EndTable();
}
