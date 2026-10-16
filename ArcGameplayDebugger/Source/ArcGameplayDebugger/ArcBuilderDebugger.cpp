// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcBuilderDebugger.h"

#include "imgui.h"
#include "ArcBuildingDefinition.h"
#include "ArcBuilderSubsystem.h"
#include "ArcBuildIngredient.h"
#include "ArcBuildSnappingMethod.h"
#include "ArcBuildResourceConsumption.h"
#include "ArcBuildRequirement.h"
#include "ArcBuildPostPlaceAction.h"
#include "ArcTargetingPreset.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "ArcMass/Config/ArcMassEntityConfigAsset.h"
#include "Engine/GameViewportClient.h"
#include "HAL/IConsoleManager.h"
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

UArcBuilderSubsystem* FArcBuilderDebugger::FindLocalPlayerBuilderSubsystem() const
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

	ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
	if (!LocalPlayer)
	{
		return nullptr;
	}

	return LocalPlayer->GetSubsystem<UArcBuilderSubsystem>();
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
	UArcBuilderSubsystem* BuilderSubsystem = FindLocalPlayerBuilderSubsystem();

	ImGui::SetNextWindowSize(ImVec2(750, 550), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Builder Debugger", &bShow))
	{
		ImGui::End();
		return;
	}

	if (!BuilderSubsystem)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "No UArcBuilderSubsystem found on local player.");
		ImGui::End();
		return;
	}

	DrawStatus(BuilderSubsystem);
	DrawPlacementControls(BuilderSubsystem);
	DrawToolBuildings(BuilderSubsystem);

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
		DrawDefinitionsList(BuilderSubsystem);
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
			DrawDefinitionDetail(SelectedDef, BuilderSubsystem);
		}
		else
		{
			ImGui::TextDisabled("Select a building definition to view details.");
		}
	}
	ImGui::EndChild();

	ImGui::End();
}

void FArcBuilderDebugger::DrawStatus(UArcBuilderSubsystem* BuilderSubsystem)
{
	// Tool registration is informational only — placement never gates on it.
	if (BuilderSubsystem->IsToolRegistered())
	{
		ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Tool Registered");
	}
	else
	{
		ImGui::TextDisabled("No Tool");
	}
}

void FArcBuilderDebugger::DrawPlacementControls(UArcBuilderSubsystem* BuilderSubsystem)
{
	if (BuilderSubsystem->IsPlacing())
	{
		ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Placement Active");
		ImGui::SameLine();
		if (BuilderSubsystem->DidMeetPlacementRequirements())
		{
			ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Requirements Met");
		}
		else
		{
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Requirements NOT Met");
		}
		ImGui::SameLine();
		if (ImGui::Button("Place Object"))
		{
			BuilderSubsystem->ConfirmPlacement();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			BuilderSubsystem->CancelPlacement();
		}
	}
	else
	{
		ImGui::TextDisabled("Not placing.");
	}

	// Cheat: bypass placement requirements + resource costs (needed when placing without inventory).
	if (IConsoleVariable* DisableReqCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("arc.Builder.DisableRequirements")))
	{
		bool bDisableRequirements = DisableReqCVar->GetBool();
		if (ImGui::Checkbox("Disable Requirements (cheat)", &bDisableRequirements))
		{
			DisableReqCVar->Set(bDisableRequirements);
		}
	}

	// Grid controls (real subsystem getters/setters — no local mirrors).
	if (ImGui::CollapsingHeader("Grid Controls"))
	{
		const char* GridModeNames[] = { "Off", "Global", "Relative" };
		int CurrentMode = static_cast<int>(BuilderSubsystem->GetGridMode());
		ImGui::SetNextItemWidth(150.f);
		if (ImGui::Combo("Grid Mode", &CurrentMode, GridModeNames, IM_ARRAYSIZE(GridModeNames)))
		{
			BuilderSubsystem->SetGridMode(static_cast<EArcBuilderGridMode>(CurrentMode));
		}

		int GridSize = BuilderSubsystem->GetGridSize();
		ImGui::SetNextItemWidth(150.f);
		if (ImGui::InputInt("Grid Size", &GridSize))
		{
			BuilderSubsystem->SetGridSize(FMath::Max(GridSize, 1));
		}
	}
}

void FArcBuilderDebugger::DrawToolBuildings(UArcBuilderSubsystem* BuilderSubsystem)
{
	if (!BuilderSubsystem->IsToolRegistered())
	{
		return;
	}

	if (ImGui::CollapsingHeader("Tool Buildings", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const TArray<TObjectPtr<UArcBuildingDefinition>>& Available = BuilderSubsystem->GetAvailableBuildings();

		if (Available.Num() == 0)
		{
			ImGui::TextDisabled("(registered tool offers no buildings)");
		}

		for (int32 i = 0; i < Available.Num(); ++i)
		{
			UArcBuildingDefinition* Def = Available[i];
			if (!Def)
			{
				continue;
			}

			ImGui::PushID(i);
			ImGui::BulletText("%s", TCHAR_TO_ANSI(*GetNameSafe(Def)));
			ImGui::SameLine();
			if (ImGui::SmallButton("Place"))
			{
				BuilderSubsystem->StartPlacement(Def);
			}
			ImGui::PopID();
		}
	}
}

void FArcBuilderDebugger::DrawDefinitionsList(UArcBuilderSubsystem* BuilderSubsystem)
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

			// Place button (always enabled — placement no longer gates on tool state)
			ImGui::TableNextColumn();
			if (ImGui::SmallButton("Place"))
			{
				if (SourceIdx >= 0 && SourceIdx < CachedBuildingDefinitions.Num())
				{
					UArcBuildingDefinition* BuildDef = Cast<UArcBuildingDefinition>(CachedBuildingDefinitions[SourceIdx].GetAsset());
					if (BuildDef)
					{
						BuilderSubsystem->StartPlacement(BuildDef);
					}
				}
			}

			ImGui::PopID();
		}

		ImGui::EndTable();
	}
}

void FArcBuilderDebugger::DrawDefinitionDetail(UArcBuildingDefinition* BuildDef, UArcBuilderSubsystem* BuilderSubsystem)
{
	// Identity
	ImGui::Text("Name: %s", TCHAR_TO_ANSI(*GetNameSafe(BuildDef)));
	ImGui::Text("Display: %s", TCHAR_TO_ANSI(*BuildDef->DisplayName.ToString()));
	if (!BuildDef->Description.IsEmpty())
	{
		ImGui::Text("Description: %s", TCHAR_TO_ANSI(*BuildDef->Description.ToString()));
	}
	ImGui::Text("BuildingId: %s", TCHAR_TO_ANSI(*BuildDef->BuildingId.ToString(EGuidFormats::DigitsWithHyphens)));
	ImGui::Text("BuildingType: %s", TCHAR_TO_ANSI(*BuildDef->BuildingType.ToString()));

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
		ImGui::Text("Icon: %s", TCHAR_TO_ANSI(*BuildDef->Icon.ToString()));
	}

	// Targeting
	if (ImGui::CollapsingHeader("Targeting"))
	{
		if (BuildDef->PlacementTargetingPreset)
		{
			ImGui::Text("PlacementPreset: %s", TCHAR_TO_ANSI(*GetNameSafe(BuildDef->PlacementTargetingPreset)));
		}
		else
		{
			ImGui::TextDisabled("(none, subsystem default preset)");
		}
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

	// Post-Place Actions
	if (ImGui::CollapsingHeader("Post-Place Actions"))
	{
		if (BuildDef->PostPlaceActions.Num() == 0)
		{
			ImGui::TextDisabled("(none)");
		}
		else
		{
			for (int32 i = 0; i < BuildDef->PostPlaceActions.Num(); ++i)
			{
				const TInstancedStruct<FArcBuildPostPlaceAction>& ActionStruct = BuildDef->PostPlaceActions[i];
				if (ActionStruct.IsValid())
				{
					ImGui::BulletText("[%d] %s", i, TCHAR_TO_ANSI(*GetNameSafe(ActionStruct.GetScriptStruct())));
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

	// Action buttons — always enabled (placement no longer gates on tool state).
	if (ImGui::Button("Begin Placement"))
	{
		BuilderSubsystem->StartPlacement(BuildDef);
	}

	if (BuilderSubsystem->IsPlacing())
	{
		ImGui::SameLine();
		if (ImGui::Button("Place Object##Detail"))
		{
			BuilderSubsystem->ConfirmPlacement();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel##Detail"))
		{
			BuilderSubsystem->CancelPlacement();
		}
	}
}
