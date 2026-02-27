// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcBuilderDebugger.h"

#include "imgui.h"
#include "ArcBuildingDefinition.h"
#include "ArcBuilderComponent.h"
#include "ArcBuildIngredient.h"
#include "ArcBuildSnappingMethod.h"
#include "ArcBuildResourceConsumption.h"
#include "ArcBuildRequirement.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "MassEntityConfigAsset.h"
#include "Engine/GameViewportClient.h"
#include "Materials/MaterialInterface.h"

void FArcBuilderDebugger::Initialize()
{
	CachedBuildingDefinitions.Reset();
	CachedBuildingDefinitionNames.Reset();
	FilteredToSourceIndex.Reset();
	SelectedDefinitionIndex = -1;
	FMemory::Memzero(FilterBuf, sizeof(FilterBuf));
}

void FArcBuilderDebugger::Uninitialize()
{
	CachedBuildingDefinitions.Reset();
	CachedBuildingDefinitionNames.Reset();
	FilteredToSourceIndex.Reset();
	SelectedDefinitionIndex = -1;
}

UArcBuilderComponent* FArcBuilderDebugger::FindLocalPlayerBuilderComponent() const
{
	if (!GEngine || !GEngine->GameViewport)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GameViewport->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC)
	{
		return nullptr;
	}

	// Search on controller, pawn, and player state.
	TArray<AActor*, TInlineAllocator<3>> ActorsToSearch;
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

void FArcBuilderDebugger::RefreshBuildingDefinitions()
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

	SelectedDefinitionIndex = -1;
	ApplyFilter();
}

void FArcBuilderDebugger::ApplyFilter()
{
	FilteredToSourceIndex.Reset();
	FString FilterStr(FilterBuf);
	FString Lower = FilterStr.ToLower();

	for (int32 i = 0; i < CachedBuildingDefinitionNames.Num(); ++i)
	{
		if (FilterStr.IsEmpty() || CachedBuildingDefinitionNames[i].ToLower().Contains(Lower))
		{
			FilteredToSourceIndex.Add(i);
		}
	}
}

void FArcBuilderDebugger::Draw()
{
	UArcBuilderComponent* BuilderComp = FindLocalPlayerBuilderComponent();

	ImGui::SetNextWindowSize(ImVec2(750, 550), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Builder Debugger", &bShow))
	{
		ImGui::End();
		return;
	}

	if (!BuilderComp)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "No UArcBuilderComponent found on local player.");
		ImGui::End();
		return;
	}

	// Placement status bar
	DrawPlacementControls(BuilderComp);

	ImGui::Separator();

	// Header with refresh
	if (ImGui::Button("Refresh"))
	{
		RefreshBuildingDefinitions();
		FMemory::Memzero(FilterBuf, sizeof(FilterBuf));
	}
	ImGui::SameLine();
	ImGui::Text("Building Definitions (%d)", CachedBuildingDefinitions.Num());

	// Lazy load
	if (CachedBuildingDefinitions.Num() == 0 && CachedBuildingDefinitionNames.Num() == 0)
	{
		RefreshBuildingDefinitions();
	}

	// Filter
	ImGui::SetNextItemWidth(250.f);
	if (ImGui::InputText("Filter", FilterBuf, sizeof(FilterBuf)))
	{
		SelectedDefinitionIndex = -1;
		ApplyFilter();
	}

	ImGui::Separator();

	// Two-column layout: list + detail
	float AvailWidth = ImGui::GetContentRegionAvail().x;
	float ListWidth = FMath::Max(AvailWidth * 0.4f, 200.f);

	// Left: definitions list
	if (ImGui::BeginChild("DefList", ImVec2(ListWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
	{
		DrawDefinitionsList(BuilderComp);
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// Right: detail panel
	if (ImGui::BeginChild("DefDetail", ImVec2(0, 0), ImGuiChildFlags_Borders))
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
			DrawDefinitionDetail(SelectedDef, BuilderComp);
		}
		else
		{
			ImGui::TextDisabled("Select a building definition to view details.");
		}
	}
	ImGui::EndChild();

	ImGui::End();
}

void FArcBuilderDebugger::DrawPlacementControls(UArcBuilderComponent* BuilderComp)
{
	if (BuilderComp->IsPlacing())
	{
		ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Placement Active");
		ImGui::SameLine();
		if (ImGui::Button("Place Object"))
		{
			BuilderComp->PlaceObject();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			BuilderComp->EndPlacement();
		}
	}
	else
	{
		ImGui::TextDisabled("Not placing.");
	}
}

void FArcBuilderDebugger::DrawDefinitionsList(UArcBuilderComponent* BuilderComp)
{
	const ImGuiTableFlags TableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit ;
	if (ImGui::BeginTable("DefsTable", 2, TableFlags))
	{
		ImGui::TableSetupColumn("Name");
		ImGui::TableSetupColumn("##Actions");
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableHeadersRow();

		for (int32 i = 0; i < FilteredToSourceIndex.Num(); ++i)
		{
			int32 SourceIdx = FilteredToSourceIndex[i];
			const FString& Name = CachedBuildingDefinitionNames[SourceIdx];

			ImGui::PushID(i);
			ImGui::TableNextRow();

			// Name column (selectable)
			ImGui::TableNextColumn();
			bool bSelected = (SelectedDefinitionIndex == i);
			if (ImGui::Selectable(TCHAR_TO_ANSI(*Name), bSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
			{
				SelectedDefinitionIndex = bSelected ? -1 : i;
			}

			// Place button
			ImGui::TableNextColumn();
			if (ImGui::SmallButton("Place"))
			{
				if (SourceIdx >= 0 && SourceIdx < CachedBuildingDefinitions.Num())
				{
					UArcBuildingDefinition* BuildDef = Cast<UArcBuildingDefinition>(CachedBuildingDefinitions[SourceIdx].GetAsset());
					if (BuildDef)
					{
						BuilderComp->BeginPlacementFromDefinition(BuildDef);
					}
				}
			}

			ImGui::PopID();
		}

		ImGui::EndTable();
	}
}

void FArcBuilderDebugger::DrawDefinitionDetail(UArcBuildingDefinition* BuildDef, UArcBuilderComponent* BuilderComp)
{
	// Identity
	ImGui::Text("Name: %s", TCHAR_TO_ANSI(*GetNameSafe(BuildDef)));
	ImGui::Text("Display: %s", TCHAR_TO_ANSI(*BuildDef->DisplayName.ToString()));
	ImGui::Text("BuildingId: %s", TCHAR_TO_ANSI(*BuildDef->BuildingId.ToString(EGuidFormats::DigitsWithHyphens)));

	if (BuildDef->BuildingTags.Num() > 0)
	{
		ImGui::Text("Tags: %s", TCHAR_TO_ANSI(*BuildDef->BuildingTags.ToStringSimple()));
	}

	ImGui::Separator();

	// Spawning
	if (ImGui::CollapsingHeader("Spawning", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("ActorClass: %s", TCHAR_TO_ANSI(*BuildDef->ActorClass.ToString()));
		ImGui::Text("MassEntityConfig: %s", TCHAR_TO_ANSI(*GetNameSafe(BuildDef->MassEntityConfig)));
	}

	// Grid
	if (ImGui::CollapsingHeader("Grid", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("DefaultGridSize: %d", BuildDef->DefaultGridSize);
		ImGui::Text("ApproximateSize: %s", TCHAR_TO_ANSI(*BuildDef->ApproximateSize.ToString()));
	}

	// Preview Materials
	if (ImGui::CollapsingHeader("Preview"))
	{
		ImGui::Text("MeetMaterial: %s", TCHAR_TO_ANSI(*GetNameSafe(BuildDef->RequirementMeetMaterial)));
		ImGui::Text("FailMaterial: %s", TCHAR_TO_ANSI(*GetNameSafe(BuildDef->RequirementFailedMaterial)));
	}

	// Ingredients
	if (ImGui::CollapsingHeader("Ingredients", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (BuildDef->Ingredients.Num() == 0)
		{
			ImGui::TextDisabled("(free placement)");
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
				ImGui::BulletText("[%d] %s | Slot: %s | Amount: %d",
					i, TCHAR_TO_ANSI(*TypeName), TCHAR_TO_ANSI(*SlotStr), Ingredient->Amount);
			}
		}
	}

	// Snapping
	if (ImGui::CollapsingHeader("Snapping"))
	{
		if (BuildDef->SnappingMethod.IsValid())
		{
			ImGui::Text("Method: %s", TCHAR_TO_ANSI(*GetNameSafe(BuildDef->SnappingMethod.GetScriptStruct())));
		}
		else
		{
			ImGui::TextDisabled("(none, grid fallback)");
		}
	}

	// Resource Consumption
	if (ImGui::CollapsingHeader("Resource Consumption"))
	{
		if (BuildDef->ResourceConsumption.IsValid())
		{
			ImGui::Text("Strategy: %s", TCHAR_TO_ANSI(*GetNameSafe(BuildDef->ResourceConsumption.GetScriptStruct())));
		}
		else
		{
			ImGui::TextDisabled("(none, free placement)");
		}
	}

	// Requirements
	if (ImGui::CollapsingHeader("Requirements"))
	{
		if (BuildDef->BuildRequirements.Num() == 0)
		{
			ImGui::TextDisabled("(none)");
		}
		else
		{
			for (int32 i = 0; i < BuildDef->BuildRequirements.Num(); ++i)
			{
				const TInstancedStruct<FArcBuildRequirement>& ReqStruct = BuildDef->BuildRequirements[i];
				if (ReqStruct.IsValid())
				{
					ImGui::BulletText("[%d] %s", i, TCHAR_TO_ANSI(*GetNameSafe(ReqStruct.GetScriptStruct())));
				}
			}
		}
	}

	// Sockets
	if (ImGui::CollapsingHeader("Sockets"))
	{
		if (BuildDef->SocketInfo.SocketNames.Num() > 0)
		{
			for (const FName& SocketName : BuildDef->SocketInfo.SocketNames)
			{
				ImGui::BulletText("%s", TCHAR_TO_ANSI(*SocketName.ToString()));
			}
		}
		else
		{
			ImGui::TextDisabled("(none)");
		}

		if (BuildDef->SocketInfo.SocketTags.Num() > 0)
		{
			ImGui::Text("SocketTags: %s", TCHAR_TO_ANSI(*BuildDef->SocketInfo.SocketTags.ToStringSimple()));
		}
	}

	ImGui::Separator();

	// Action buttons
	if (ImGui::Button("Begin Placement"))
	{
		BuilderComp->BeginPlacementFromDefinition(BuildDef);
	}

	if (BuilderComp->IsPlacing())
	{
		ImGui::SameLine();
		if (ImGui::Button("Place Object##Detail"))
		{
			BuilderComp->PlaceObject();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel##Detail"))
		{
			BuilderComp->EndPlacement();
		}
	}
}
