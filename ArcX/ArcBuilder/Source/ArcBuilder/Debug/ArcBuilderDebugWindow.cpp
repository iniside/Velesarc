// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcBuilderDebugWindow.h"

#include "SlateIM.h"
#include "MassEntityConfigAsset.h"
#include "Materials/MaterialInterface.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Styling/AppStyle.h"

#include "ArcBuildingDefinition.h"
#include "ArcBuilderComponent.h"
#include "ArcBuildIngredient.h"

FArcBuilderDebugWindow::FArcBuilderDebugWindow()
	: FSlateIMWindowBase(
		TEXT("Arc Builder Debug"),
		FVector2f(700.f, 500.f),
		TEXT("Arc.Builder.Debug"),
		TEXT("Toggle the Arc Builder debug window"))
{
}

UArcBuilderComponent* FArcBuilderDebugWindow::FindLocalPlayerBuilderComponent() const
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

	// Search on controller, pawn, and player state.
	TArray<AActor*> ActorsToSearch;
	ActorsToSearch.Add(PC);
	if (APawn* Pawn = PC->GetPawn())
	{
		ActorsToSearch.Add(Pawn);
	}
	if (APlayerState* PS = PC->GetPlayerState<APlayerState>())
	{
		ActorsToSearch.Add(PS);
	}

	for (AActor* Actor : ActorsToSearch)
	{
		if (UArcBuilderComponent* Comp = Actor->FindComponentByClass<UArcBuilderComponent>())
		{
			return Comp;
		}
	}

	return nullptr;
}

void FArcBuilderDebugWindow::RefreshBuildingDefinitions()
{
	CachedBuildingDefinitions.Reset();
	CachedBuildingDefinitionNames.Reset();

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	AssetRegistry.GetAssetsByClass(UArcBuildingDefinition::StaticClass()->GetClassPathName(), CachedBuildingDefinitions, true);

	for (const FAssetData& AssetData : CachedBuildingDefinitions)
	{
		CachedBuildingDefinitionNames.Add(AssetData.AssetName.ToString());
	}

	// Reset filter to show all.
	FilteredDefinitionNames = CachedBuildingDefinitionNames;
	FilteredToSourceIndex.Reset();
	for (int32 i = 0; i < CachedBuildingDefinitionNames.Num(); ++i)
	{
		FilteredToSourceIndex.Add(i);
	}
}

void FArcBuilderDebugWindow::DrawWindow(float DeltaTime)
{
	SlateIM::Fill();
	SlateIM::HAlign(HAlign_Fill);
	SlateIM::VAlign(VAlign_Fill);
	SlateIM::BeginBorder(FAppStyle::GetBrush("ToolPanel.GroupBorder"));
	SlateIM::Fill();
	SlateIM::HAlign(HAlign_Fill);
	SlateIM::VAlign(VAlign_Fill);
	SlateIM::BeginVerticalStack();
	{
		// Placement controls bar (visible when actively placing).
		UArcBuilderComponent* BuilderComp = FindLocalPlayerBuilderComponent();
		if (BuilderComp && BuilderComp->IsPlacing())
		{
			SlateIM::Padding(FMargin(4.f));
			SlateIM::BeginHorizontalStack();
			{
				SlateIM::Text(TEXT("Placement active"));
				SlateIM::Padding(FMargin(8.f, 0.f));
				if (SlateIM::Button(TEXT("Place Object")))
				{
					BuilderComp->PlaceObject();
				}
				SlateIM::Padding(FMargin(4.f, 0.f));
				if (SlateIM::Button(TEXT("Cancel")))
				{
					BuilderComp->EndPlacement();
				}
			}
			SlateIM::EndHorizontalStack();
		}

		// Header row with refresh button.
		SlateIM::Padding(FMargin(4.f));
		SlateIM::BeginHorizontalStack();
		{
			SlateIM::Text(TEXT("Building Definitions"));
			SlateIM::Padding(FMargin(8.f, 0.f));
			if (SlateIM::Button(TEXT("Refresh")))
			{
				RefreshBuildingDefinitions();
				FilterText.Empty();
			}
		}
		SlateIM::EndHorizontalStack();

		// Lazy load on first frame.
		if (CachedBuildingDefinitions.Num() == 0 && CachedBuildingDefinitionNames.Num() == 0)
		{
			RefreshBuildingDefinitions();
		}

		// Filter.
		SlateIM::Padding(FMargin(4.f));
		SlateIM::BeginHorizontalStack();
		{
			SlateIM::Text(TEXT("Filter: "));
			SlateIM::MinWidth(200.f);
			if (SlateIM::EditableText(FilterText, TEXT("Search...")))
			{
				FilteredDefinitionNames.Reset();
				FilteredToSourceIndex.Reset();
				FString Lower = FilterText.ToLower();
				for (int32 i = 0; i < CachedBuildingDefinitionNames.Num(); ++i)
				{
					if (FilterText.IsEmpty() || CachedBuildingDefinitionNames[i].ToLower().Contains(Lower))
					{
						FilteredDefinitionNames.Add(CachedBuildingDefinitionNames[i]);
						FilteredToSourceIndex.Add(i);
					}
				}
				SelectedDefinitionIndex = -1;
			}
		}
		SlateIM::EndHorizontalStack();

		// Main content: left list + right detail.
		SlateIM::Padding(FMargin(4.f));
		SlateIM::Fill();
		SlateIM::BeginHorizontalStack();
		{
			// Left: building definitions list.
			SlateIM::Fill();
			SlateIM::HAlign(HAlign_Fill);
			SlateIM::VAlign(VAlign_Fill);
			SlateIM::BeginVerticalStack();
			{
				DrawBuildingDefinitionsList();
			}
			SlateIM::EndVerticalStack();

			// Right: detail panel.
			SlateIM::Fill();
			SlateIM::HAlign(HAlign_Fill);
			SlateIM::VAlign(VAlign_Fill);
			SlateIM::BeginVerticalStack();
			{
				UArcBuildingDefinition* SelectedDef = nullptr;
				if (SelectedDefinitionIndex >= 0 && SelectedDefinitionIndex < FilteredToSourceIndex.Num())
				{
					int32 SourceIdx = FilteredToSourceIndex[SelectedDefinitionIndex];
					if (SourceIdx >= 0 && SourceIdx < CachedBuildingDefinitions.Num())
					{
						SelectedDef = Cast<UArcBuildingDefinition>(CachedBuildingDefinitions[SourceIdx].GetAsset());
					}
				}

				if (SelectedDef)
				{
					DrawBuildingDefinitionDetail(SelectedDef);
				}
				else
				{
					SlateIM::Padding(FMargin(8.f));
					SlateIM::Text(TEXT("Select a building definition to view details"));
				}
			}
			SlateIM::EndVerticalStack();
		}
		SlateIM::EndHorizontalStack();
	}
	SlateIM::EndVerticalStack();
	SlateIM::EndBorder();
}

void FArcBuilderDebugWindow::DrawBuildingDefinitionsList()
{
	SlateIM::Padding(FMargin(4.f));
	SlateIM::Fill();
	SlateIM::FTableParams TableParams;
	TableParams.SelectionMode = ESelectionMode::Single;
	SlateIM::BeginTable(TableParams);
	{
		SlateIM::BeginTableHeader();
		{
			SlateIM::InitialTableColumnWidth(200.f);
			SlateIM::AddTableColumn(TEXT("Name"), TEXT("Name"));
			SlateIM::FixedTableColumnWidth(70.f);
			SlateIM::AddTableColumn(TEXT("Actions"), TEXT("Actions"));
		}
		SlateIM::EndTableHeader();

		SlateIM::BeginTableBody();
		{
			for (int32 i = 0; i < FilteredDefinitionNames.Num(); ++i)
			{
				bool bRowSelected = false;

				// Name column.
				if (SlateIM::NextTableCell(&bRowSelected))
				{
					SlateIM::Text(*FilteredDefinitionNames[i]);
				}

				if (bRowSelected)
				{
					SelectedDefinitionIndex = i;
				}

				// Actions column.
				if (SlateIM::NextTableCell())
				{
					if (SlateIM::Button(*FString::Printf(TEXT("Place##%d"), i)))
					{
						int32 SourceIdx = FilteredToSourceIndex[i];
						if (SourceIdx >= 0 && SourceIdx < CachedBuildingDefinitions.Num())
						{
							UArcBuildingDefinition* BuildDef = Cast<UArcBuildingDefinition>(CachedBuildingDefinitions[SourceIdx].GetAsset());
							if (BuildDef)
							{
								UArcBuilderComponent* BuilderComp = FindLocalPlayerBuilderComponent();
								if (BuilderComp)
								{
									BuilderComp->BeginPlacementFromDefinition(BuildDef);
								}
							}
						}
					}
				}
			}
		}
		SlateIM::EndTableBody();
	}
	SlateIM::EndTable();
}

void FArcBuilderDebugWindow::DrawBuildingDefinitionDetail(UArcBuildingDefinition* BuildDef)
{
	if (!BuildDef)
	{
		return;
	}

	SlateIM::Padding(FMargin(4.f));
	SlateIM::BeginScrollBox();
	{
		// Identity.
		SlateIM::Text(*FString::Printf(TEXT("Name: %s"), *GetNameSafe(BuildDef)));
		SlateIM::Text(*FString::Printf(TEXT("Display: %s"), *BuildDef->DisplayName.ToString()));
		SlateIM::Text(*FString::Printf(TEXT("BuildingId: %s"), *BuildDef->BuildingId.ToString(EGuidFormats::DigitsWithHyphens)));

		if (BuildDef->BuildingTags.Num() > 0)
		{
			SlateIM::Text(*FString::Printf(TEXT("Tags: %s"), *BuildDef->BuildingTags.ToString()));
		}

		// Spawning.
		SlateIM::Spacer(FVector2D(0.0, 8.0));
		SlateIM::Text(TEXT("--- Spawning ---"));
		SlateIM::Text(*FString::Printf(TEXT("ActorClass: %s"), *BuildDef->ActorClass.ToString()));
		SlateIM::Text(*FString::Printf(TEXT("MassEntityConfig: %s"), *GetNameSafe(BuildDef->MassEntityConfig)));

		// Grid.
		SlateIM::Spacer(FVector2D(0.0, 8.0));
		SlateIM::Text(TEXT("--- Grid ---"));
		SlateIM::Text(*FString::Printf(TEXT("DefaultGridSize: %d"), BuildDef->DefaultGridSize));
		SlateIM::Text(*FString::Printf(TEXT("ApproximateSize: %s"), *BuildDef->ApproximateSize.ToString()));

		// Materials.
		SlateIM::Spacer(FVector2D(0.0, 8.0));
		SlateIM::Text(TEXT("--- Preview ---"));
		SlateIM::Text(*FString::Printf(TEXT("MeetMaterial: %s"), *GetNameSafe(BuildDef->RequirementMeetMaterial)));
		SlateIM::Text(*FString::Printf(TEXT("FailMaterial: %s"), *GetNameSafe(BuildDef->RequirementFailedMaterial)));

		// Ingredients.
		SlateIM::Spacer(FVector2D(0.0, 8.0));
		SlateIM::Text(*FString::Printf(TEXT("--- Ingredients (%d) ---"), BuildDef->Ingredients.Num()));
		if (BuildDef->Ingredients.Num() == 0)
		{
			SlateIM::Padding(FMargin(8.f, 2.f));
			SlateIM::Text(TEXT("(free placement)"));
		}
		else
		{
			for (int32 i = 0; i < BuildDef->Ingredients.Num(); ++i)
			{
				const FInstancedStruct& IngredientStruct = BuildDef->Ingredients[i];
				if (!IngredientStruct.IsValid())
				{
					continue;
				}

				const FArcBuildIngredient* Ingredient = IngredientStruct.GetPtr<FArcBuildIngredient>();
				if (!Ingredient)
				{
					continue;
				}

				FString TypeName = GetNameSafe(IngredientStruct.GetScriptStruct());
				FString SlotStr = Ingredient->SlotName.IsEmpty() ? TEXT("-") : Ingredient->SlotName.ToString();
				SlateIM::Text(*FString::Printf(TEXT("  [%d] %s | Slot: %s | Amount: %d"),
					i, *TypeName, *SlotStr, Ingredient->Amount));
			}
		}

		// Snapping.
		SlateIM::Spacer(FVector2D(0.0, 8.0));
		SlateIM::Text(TEXT("--- Snapping ---"));
		if (BuildDef->SnappingMethod.IsValid())
		{
			SlateIM::Text(*FString::Printf(TEXT("Method: %s"), *GetNameSafe(BuildDef->SnappingMethod.GetScriptStruct())));
		}
		else
		{
			SlateIM::Text(TEXT("(none, grid fallback)"));
		}

		// Consumption.
		SlateIM::Spacer(FVector2D(0.0, 8.0));
		SlateIM::Text(TEXT("--- Resource Consumption ---"));
		if (BuildDef->ResourceConsumption.IsValid())
		{
			SlateIM::Text(*FString::Printf(TEXT("Strategy: %s"), *GetNameSafe(BuildDef->ResourceConsumption.GetScriptStruct())));
		}
		else
		{
			SlateIM::Text(TEXT("(none, free placement)"));
		}

		// Requirements.
		SlateIM::Spacer(FVector2D(0.0, 8.0));
		SlateIM::Text(*FString::Printf(TEXT("--- Requirements (%d) ---"), BuildDef->BuildRequirements.Num()));
		for (int32 i = 0; i < BuildDef->BuildRequirements.Num(); ++i)
		{
			const TInstancedStruct<FArcBuildRequirement>& ReqStruct = BuildDef->BuildRequirements[i];
			if (ReqStruct.IsValid())
			{
				SlateIM::Text(*FString::Printf(TEXT("  [%d] %s"), i, *GetNameSafe(ReqStruct.GetScriptStruct())));
			}
		}

		// Socket info.
		SlateIM::Spacer(FVector2D(0.0, 8.0));
		SlateIM::Text(TEXT("--- Sockets ---"));
		if (BuildDef->SocketInfo.SocketNames.Num() > 0)
		{
			for (const FName& SocketName : BuildDef->SocketInfo.SocketNames)
			{
				SlateIM::Text(*FString::Printf(TEXT("  %s"), *SocketName.ToString()));
			}
		}
		else
		{
			SlateIM::Text(TEXT("(none)"));
		}

		if (BuildDef->SocketInfo.SocketTags.Num() > 0)
		{
			SlateIM::Text(*FString::Printf(TEXT("  SocketTags: %s"), *BuildDef->SocketInfo.SocketTags.ToString()));
		}

		// Actions at the bottom.
		SlateIM::Spacer(FVector2D(0.0, 12.0));
		UArcBuilderComponent* DetailBuilderComp = FindLocalPlayerBuilderComponent();
		if (DetailBuilderComp)
		{
			SlateIM::BeginHorizontalStack();
			{
				if (SlateIM::Button(TEXT("Begin Placement")))
				{
					DetailBuilderComp->BeginPlacementFromDefinition(BuildDef);
				}

				if (DetailBuilderComp->IsPlacing())
				{
					SlateIM::Padding(FMargin(4.f, 0.f));
					if (SlateIM::Button(TEXT("Place Object")))
					{
						DetailBuilderComp->PlaceObject();
					}
					SlateIM::Padding(FMargin(4.f, 0.f));
					if (SlateIM::Button(TEXT("Cancel")))
					{
						DetailBuilderComp->EndPlacement();
					}
				}
			}
			SlateIM::EndHorizontalStack();
		}
	}
	SlateIM::EndScrollBox();
}
