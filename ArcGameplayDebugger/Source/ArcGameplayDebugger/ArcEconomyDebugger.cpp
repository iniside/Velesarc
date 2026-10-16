// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEconomyDebugger.h"

#include "ArcEconomyDebuggerDrawComponent.h"
#include "ArcMass/Config/ArcMassEntityConfigAsset.h"
#include "ArcMass/Config/ArcMassEntityTraitBase.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "MassEntitySubsystem.h"
#include "MassEntityTemplate.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityManager.h"
#include "Sim/ArcEconomyNames.h"
#include "Sim/ArcGameDirectorSubsystem.h"
#include "Sim/ArcSettlementBounds.h"
#include "Sim/ArcSettlementLedgerSubsystem.h"
#include "Sim/ArcSettlementLedgerTypes.h"
#include "ArcMass/Spawner/ArcMassSpawnerSubsystem.h"
#include "imgui.h"

namespace ArcEconomyDebuggerSpawnDiag
{
	FString DescribeConfig(const UArcMassEntityConfigAsset& Config, const UWorld& World)
	{
		TArray<UMassEntityTraitBase*> Traits;
		ArcMass::CollectCombinedTraits(Config.GetConfig(), &World, Traits);

		const UMassEntityConfigAsset* Parent = Config.GetConfig().GetParent();

		FString Out = FString::Printf(TEXT("Config: %s (parent: %s)\n"),
			*Config.GetName(), Parent != nullptr ? *Parent->GetName() : TEXT("none"));

		Out += FString::Printf(TEXT("Traits over the whole parent chain (%d):\n"), Traits.Num());
		for (const UMassEntityTraitBase* Trait : Traits)
		{
			Out += FString::Printf(TEXT("  %s\n"), *Trait->GetClass()->GetName());
		}

		const FMassEntityTemplate& Template = Config.GetConfig().GetOrCreateEntityTemplate(World);
		const FMassFragmentBitSet FragmentBits = Template.GetCompositionDescriptor().DebugGetFragments();
		TArray<const UScriptStruct*> FragmentTypes;
		FragmentBits.ExportTypes(FragmentTypes);

		bool bHasBodyFragment = false;
		bool bHasMembershipFragment = false;
		bool bHasWorkAssignmentFragment = false;
		bool bHasGatherAssignmentFragment = false;
		bool bHasCarrierAssignmentFragment = false;
		bool bHasCarryCapabilityFragment = false;
		Out += FString::Printf(TEXT("Template fragments (%d):\n"), FragmentTypes.Num());
		for (const UScriptStruct* FragmentType : FragmentTypes)
		{
			if (FragmentType == nullptr)
			{
				continue;
			}
			Out += FString::Printf(TEXT("  %s\n"), *FragmentType->GetName());
			bHasBodyFragment = bHasBodyFragment || (FragmentType == FArcBodyFragment::StaticStruct());
			bHasMembershipFragment = bHasMembershipFragment
				|| (FragmentType == FArcSettlementMembershipFragment::StaticStruct());
			bHasWorkAssignmentFragment = bHasWorkAssignmentFragment
				|| (FragmentType == FArcWorkAssignmentFragment::StaticStruct());
			bHasGatherAssignmentFragment = bHasGatherAssignmentFragment
				|| (FragmentType == FArcGatherAssignmentFragment::StaticStruct());
			bHasCarrierAssignmentFragment = bHasCarrierAssignmentFragment
				|| (FragmentType == FArcCarrierAssignmentFragment::StaticStruct());
			bHasCarryCapabilityFragment = bHasCarryCapabilityFragment
				|| (FragmentType == FArcCarryCapabilityFragment::StaticStruct());
		}
		const bool bHasAssignmentFragments = bHasMembershipFragment
			&& bHasWorkAssignmentFragment
			&& bHasGatherAssignmentFragment
			&& bHasCarrierAssignmentFragment
			&& bHasCarryCapabilityFragment;
		Out += FString::Printf(TEXT("FArcBodyFragment present: %s\n"), bHasBodyFragment ? TEXT("yes") : TEXT("no"));
		Out += FString::Printf(TEXT("Employment fragments present: %s\n"),
			bHasAssignmentFragments ? TEXT("yes") : TEXT("no"));

		FArcSparseFragmentBuildContext SparseContext;
		for (const UMassEntityTraitBase* Trait : Traits)
		{
			const UArcMassEntityTraitBase* ArcTrait = Cast<const UArcMassEntityTraitBase>(Trait);
			if (ArcTrait != nullptr)
			{
				ArcTrait->BuildSparseFragments(SparseContext, World);
			}
		}

		Out += FString::Printf(TEXT("Sparse fragments (%d):\n"), SparseContext.SparseFragments.Num());
		for (const FInstancedStruct& SparseFragment : SparseContext.SparseFragments)
		{
			Out += FString::Printf(TEXT("  %s\n"),
				SparseFragment.GetScriptStruct() != nullptr ? *SparseFragment.GetScriptStruct()->GetName() : TEXT("<null>"));
		}
		for (const UScriptStruct* TagType : SparseContext.SparseTagTypes)
		{
			Out += FString::Printf(TEXT("  tag %s\n"), TagType != nullptr ? *TagType->GetName() : TEXT("<null>"));
		}

		return Out;
	}
}

namespace ArcEconomyDebuggerShared
{
	UWorld* GetDebugWorld()
	{
		if (!GEngine || !GEngine->GameViewport)
		{
			return nullptr;
		}
		return GEngine->GameViewport->GetWorld();
	}

	FMassEntityManager* GetEntityManager()
	{
		UWorld* World = GetDebugWorld();
		if (!World)
		{
			return nullptr;
		}
		UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>();
		if (!MassSubsystem)
		{
			return nullptr;
		}
		return &MassSubsystem->GetMutableEntityManager();
	}

	UArcSettlementLedgerSubsystem* GetLedger()
	{
		UWorld* World = GetDebugWorld();
		if (!World)
		{
			return nullptr;
		}
		return World->GetSubsystem<UArcSettlementLedgerSubsystem>();
	}

	const char* ProductionStateName(EArcProductionState State)
	{
		switch (State)
		{
		case EArcProductionState::Unstaffed: return "Unstaffed";
		case EArcProductionState::Starved:   return "Starved";
		case EArcProductionState::Producing: return "Producing";
		case EArcProductionState::Blocked:   return "Blocked";
		default:                             return "?";
		}
	}

	const char* GatheringStateName(EArcGatheringState State)
	{
		switch (State)
		{
		case EArcGatheringState::Unstaffed: return "Unstaffed";
		case EArcGatheringState::Gathering: return "Gathering";
		case EArcGatheringState::Blocked:   return "Blocked";
		default:                            return "?";
		}
	}

	const char* BodyStateName(EArcBodyState State)
	{
		switch (State)
		{
		case EArcBodyState::Idle:        return "Idle";
		case EArcBodyState::MoveTo:      return "MoveTo";
		case EArcBodyState::Interaction: return "Interaction";
		default:                         return "?";
		}
	}

	FString EntityLabel(const FMassEntityManager& Manager, FMassEntityHandle Entity)
	{
		return ArcEconomyNames::BuildingName(Manager, Entity);
	}
}

namespace ArcEconomyDebuggerMainPrivate
{
	FString JoinItemNames(const TArray<FPrimaryAssetId>& Items)
	{
		FString Result;
		for (const FPrimaryAssetId& Item : Items)
		{
			if (!Result.IsEmpty())
			{
				Result += TEXT(", ");
			}
			Result += ArcEconomyNames::ItemName(Item);
		}
		return Result.IsEmpty() ? FString(TEXT("(empty)")) : Result;
	}

	void DrawHaveTable(const char* TableId, const FMassEntityManager& Manager, const TArray<FArcHaveRow>& Rows)
	{
		if (Rows.IsEmpty())
		{
			ImGui::TextDisabled("(none)");
			return;
		}

		constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter
			| ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingStretchProp;
		if (!ImGui::BeginTable(TableId, 3, TableFlags))
		{
			return;
		}

		ImGui::TableSetupColumn("Place", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Qty", ImGuiTableColumnFlags_WidthFixed, 60.0f);
		ImGui::TableHeadersRow();

		for (const FArcHaveRow& Row : Rows)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%s", TCHAR_TO_ANSI(*ArcEconomyDebuggerShared::EntityLabel(Manager, Row.Place)));
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%s", TCHAR_TO_ANSI(*ArcEconomyNames::ItemName(Row.Item)));
			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%d", Row.Quantity);
		}

		ImGui::EndTable();
	}
}

void FArcEconomyDebugger::Initialize()
{
	CachedBuildings.Reset();
	CachedNPCs.Reset();
	ChainNodes.Reset();
	ChainEdges.Reset();
	ChainDiagnostics.Reset();
	ManualBoxPositions.Reset();
	SelectedBuildingIdx = INDEX_NONE;
	SelectedNPCIdx = INDEX_NONE;
	SpawnBuildingIdx = INDEX_NONE;
	SelectedEntityConfigIdx = INDEX_NONE;
	DraggedBoxKey.Reset();
	SelectedGraphNode.Reset();
	bDragExceededThreshold = false;
	bPanningCanvas = false;
	GraphViewOffset = FVector2D::ZeroVector;
	HoveredNodeKey.Reset();
	LastSpawnMessage.Reset();
	LastRefreshTime = 0.0f;
	RefreshEntityConfigs();
	RefreshAll();
}

void FArcEconomyDebugger::Uninitialize()
{
	CachedBuildings.Reset();
	CachedNPCs.Reset();
	CachedSettlements.Reset();
	ChainNodes.Reset();
	ChainEdges.Reset();
	ChainDiagnostics.Reset();
	ManualBoxPositions.Reset();
	DraggedBoxKey.Reset();
	SelectedGraphNode.Reset();
	HoveredNodeKey.Reset();
	CachedEntityConfigs.Reset();
	CachedEntityConfigNames.Reset();
	SelectedBuildingIdx = INDEX_NONE;
	SelectedNPCIdx = INDEX_NONE;
	SelectedSettlementIdx = INDEX_NONE;
	SpawnBuildingIdx = INDEX_NONE;
	SelectedEntityConfigIdx = INDEX_NONE;
	DestroyDrawActor();
}

void FArcEconomyDebugger::RefreshAll()
{
	RefreshBuildingList();
	RefreshSettlementList();
	RefreshNPCList();
	RefreshProductionChain();
}

int32 FArcEconomyDebugger::FindBuildingIndex(FMassEntityHandle Building) const
{
	for (int32 Index = 0; Index < CachedBuildings.Num(); ++Index)
	{
		if (CachedBuildings[Index].Entity == Building)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void FArcEconomyDebugger::Draw()
{
#if WITH_MASSENTITY_DEBUG
	ImGui::SetNextWindowSize(ImVec2(1100, 620), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Economy Debugger", &bShow))
	{
		ImGui::End();
		return;
	}

	FMassEntityManager* Manager = ArcEconomyDebuggerShared::GetEntityManager();
	if (!Manager)
	{
		ImGui::TextDisabled("No MassEntitySubsystem available");
		ImGui::End();
		return;
	}

	UWorld* World = ArcEconomyDebuggerShared::GetDebugWorld();
	if (World)
	{
		const float CurrentTime = World->GetTimeSeconds();
		if (CurrentTime - LastRefreshTime > 0.5f)
		{
			LastRefreshTime = CurrentTime;
			RefreshAll();
		}
	}

	ImGui::Checkbox("Buildings", &bDrawBuildings);
	ImGui::SameLine();
	ImGui::Checkbox("NPCs", &bDrawNPCs);
	ImGui::SameLine();
	ImGui::Checkbox("Links", &bDrawLinks);
	ImGui::SameLine();
	ImGui::Checkbox("Labels", &bDrawLabels);
	ImGui::SameLine();
	if (ImGui::Button("Refresh"))
	{
		RefreshAll();
	}
	ImGui::SameLine();
	ImGui::Text("Buildings: %d  Bodies: %d", CachedBuildings.Num(), CachedNPCs.Num());

	ImGui::Separator();

	if (ImGui::BeginTabBar("EconomyTabs"))
	{
		if (ImGui::BeginTabItem("Buildings"))
		{
			DrawBuildingsTab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Workforce"))
		{
			DrawWorkforceTab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Production Chain"))
		{
			DrawProductionChainTab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Ledger + Spawn"))
		{
			DrawLedgerTab();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	PickNPCInViewport(*Manager);
	UpdateDrawComponent();

	ImGui::End();
#endif
}

void FArcEconomyDebugger::PickNPCInViewport(const FMassEntityManager& Manager)
{
#if WITH_MASSENTITY_DEBUG
	UWorld* World = ArcEconomyDebuggerShared::GetDebugWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	APlayerController* PickPC = GameInstance ? GameInstance->GetFirstLocalPlayerController(World) : nullptr;
	if (!PickPC || ImGui::GetIO().WantCaptureMouse || !ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		return;
	}

	constexpr double PickRadiusPixels = 20.0;

	const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	const ImVec2 ViewportOrigin = MainViewport ? MainViewport->Pos : ImVec2(0.0f, 0.0f);
	const ImVec2 MouseScreenPos = ImGui::GetMousePos();
	const FVector2D MousePos(static_cast<double>(MouseScreenPos.x - ViewportOrigin.x),
		static_cast<double>(MouseScreenPos.y - ViewportOrigin.y));

	int32 NearestIdx = INDEX_NONE;
	double NearestDistSq = PickRadiusPixels * PickRadiusPixels;

	for (int32 Index = 0; Index < CachedNPCs.Num(); ++Index)
	{
		const FNPCEntry& NPC = CachedNPCs[Index];
		if (!Manager.IsEntityActive(NPC.Entity))
		{
			continue;
		}

		const FTransformFragment* Transform = Manager.GetFragmentDataPtr<FTransformFragment>(NPC.Entity);
		if (!Transform)
		{
			continue;
		}

		FVector2D ProjectedPos = FVector2D::ZeroVector;
		if (!PickPC->ProjectWorldLocationToScreen(Transform->GetTransform().GetLocation(), ProjectedPos))
		{
			continue;
		}

		const double DistSq = FVector2D::DistSquared(ProjectedPos, MousePos);
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			NearestIdx = Index;
		}
	}

	if (NearestIdx != INDEX_NONE)
	{
		SelectedNPCIdx = NearestIdx;
	}
#endif
}

void FArcEconomyDebugger::DrawLedgerTab()
{
#if WITH_MASSENTITY_DEBUG
	FMassEntityManager* Manager = ArcEconomyDebuggerShared::GetEntityManager();
	UArcSettlementLedgerSubsystem* Ledger = ArcEconomyDebuggerShared::GetLedger();
	if (!Manager)
	{
		return;
	}

	if (!Ledger)
	{
		ImGui::TextDisabled("No UArcSettlementLedgerSubsystem in this world");
	}
	else
	{
		ImGui::SeparatorText("Needs");
		if (Ledger->Needs.IsEmpty())
		{
			ImGui::TextDisabled("(none)");
		}
		else
		{
			constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter
				| ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingStretchProp;
			if (ImGui::BeginTable("LedgerNeeds", 3, TableFlags))
			{
				ImGui::TableSetupColumn("Building", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("AcceptableItems", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Qty", ImGuiTableColumnFlags_WidthFixed, 60.0f);
				ImGui::TableHeadersRow();

				for (const FArcNeedRow& Need : Ledger->Needs)
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("%s", TCHAR_TO_ANSI(*ArcEconomyNames::BuildingName(*Manager, Need.Building)));
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%s", TCHAR_TO_ANSI(*ArcEconomyDebuggerMainPrivate::JoinItemNames(Need.AcceptableItems)));
					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%d", Need.Quantity);
				}

				ImGui::EndTable();
			}
		}

		ImGui::SeparatorText("Haves");
		ArcEconomyDebuggerMainPrivate::DrawHaveTable("LedgerHaves", *Manager, Ledger->Haves);

		ImGui::SeparatorText("Storage Haves");
		ArcEconomyDebuggerMainPrivate::DrawHaveTable("LedgerStorageHaves", *Manager, Ledger->StorageHaves);
	}

	ImGui::SeparatorText("Transport jobs");
	{
		constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter
			| ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingStretchProp;
		if (ImGui::BeginTable("LedgerTransportJobs", 6, TableFlags))
		{
			ImGui::TableSetupColumn("Carrier", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 130.0f);
			ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Sink", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Order", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Cargo", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableHeadersRow();

			for (const FNPCEntry& NPC : CachedNPCs)
			{
				if (!NPC.bHasTransportJob)
				{
					continue;
				}

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("%s", TCHAR_TO_ANSI(*NPC.Label));
				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%s", TCHAR_TO_ANSI(*NPC.StateText));
				ImGui::TableSetColumnIndex(2);
				ImGui::Text("%s", TCHAR_TO_ANSI(*ArcEconomyNames::BuildingName(*Manager, NPC.Source)));
				ImGui::TableSetColumnIndex(3);
				ImGui::Text("%s", TCHAR_TO_ANSI(*ArcEconomyNames::BuildingName(*Manager, NPC.Sink)));
				ImGui::TableSetColumnIndex(4);
				ImGui::Text("%s x%d", TCHAR_TO_ANSI(*ArcEconomyNames::ItemName(NPC.Item)), NPC.Quantity);
				ImGui::TableSetColumnIndex(5);
				if (NPC.Cargo.IsEmpty())
				{
					ImGui::TextDisabled("---");
				}
				else
				{
					for (const FStoreLine& Line : NPC.Cargo)
					{
						ImGui::Text("%s x%d", TCHAR_TO_ANSI(*ArcEconomyNames::ItemName(Line.Item)), Line.Count);
					}
				}
			}

			ImGui::EndTable();
		}
	}

	ImGui::SeparatorText("Spawn");
	DrawSpawnSection();
#endif
}

void FArcEconomyDebugger::RefreshEntityConfigs()
{
	CachedEntityConfigs.Reset();
	CachedEntityConfigNames.Reset();
	SelectedEntityConfigIdx = INDEX_NONE;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	AssetRegistry.GetAssetsByClass(UArcMassEntityConfigAsset::StaticClass()->GetClassPathName(), CachedEntityConfigs, true);

	for (const FAssetData& AssetData : CachedEntityConfigs)
	{
		CachedEntityConfigNames.Add(AssetData.AssetName.ToString());
	}

	if (CachedEntityConfigs.Num() > 0)
	{
		SelectedEntityConfigIdx = 0;
	}
}

void FArcEconomyDebugger::DrawSpawnSection()
{
#if WITH_MASSENTITY_DEBUG
	if (ImGui::Button("Reload configs"))
	{
		RefreshEntityConfigs();
	}
	ImGui::SameLine();
	ImGui::Text("Entity configs: %d", CachedEntityConfigs.Num());

	const FString ConfigPreview = CachedEntityConfigNames.IsValidIndex(SelectedEntityConfigIdx)
		? CachedEntityConfigNames[SelectedEntityConfigIdx]
		: FString(TEXT("(select)"));
	if (ImGui::BeginCombo("Config", TCHAR_TO_ANSI(*ConfigPreview)))
	{
		for (int32 Index = 0; Index < CachedEntityConfigNames.Num(); ++Index)
		{
			const bool bSelected = (SelectedEntityConfigIdx == Index);
			if (ImGui::Selectable(TCHAR_TO_ANSI(*CachedEntityConfigNames[Index]), bSelected))
			{
				SelectedEntityConfigIdx = Index;
			}
		}
		ImGui::EndCombo();
	}

	const char* BodyKindNames[] = { "Worker", "Gatherer", "Carrier" };
	ImGui::Combo("Body kind", &SpawnBodyKindIdx, BodyKindNames, IM_ARRAYSIZE(BodyKindNames));

	const FString BuildingPreview = CachedBuildings.IsValidIndex(SpawnBuildingIdx)
		? CachedBuildings[SpawnBuildingIdx].Name
		: FString(TEXT("(select)"));
	if (ImGui::BeginCombo("Target building", TCHAR_TO_ANSI(*BuildingPreview)))
	{
		for (int32 Index = 0; Index < CachedBuildings.Num(); ++Index)
		{
			if (CachedBuildings[Index].bGatherArea)
			{
				continue;
			}

			const bool bSelected = (SpawnBuildingIdx == Index);
			if (ImGui::Selectable(TCHAR_TO_ANSI(*CachedBuildings[Index].Name), bSelected))
			{
				SpawnBuildingIdx = Index;
			}
		}
		ImGui::EndCombo();
	}

	const FString SettlementPreview = CachedSettlements.IsValidIndex(SelectedSettlementIdx)
		? CachedSettlements[SelectedSettlementIdx].Name
		: FString(TEXT("(select)"));
	if (ImGui::BeginCombo("Settlement", TCHAR_TO_ANSI(*SettlementPreview)))
	{
		for (int32 Index = 0; Index < CachedSettlements.Num(); ++Index)
		{
			const bool bSelected = (SelectedSettlementIdx == Index);
			if (ImGui::Selectable(TCHAR_TO_ANSI(*CachedSettlements[Index].Name), bSelected))
			{
				SelectedSettlementIdx = Index;
			}
		}
		ImGui::EndCombo();
	}

	if (ImGui::InputInt("Unassigned count", &SpawnUnassignedCount))
	{
		SpawnUnassignedCount = FMath::Max(1, SpawnUnassignedCount);
	}

	const bool bCanSpawnUnassigned = CachedSettlements.IsValidIndex(SelectedSettlementIdx)
		&& CachedEntityConfigs.IsValidIndex(SelectedEntityConfigIdx);
	ImGui::BeginDisabled(!bCanSpawnUnassigned);
	if (ImGui::Button("Spawn N unassigned"))
	{
		LastSpawnMessage.Reset();

		UWorld* World = ArcEconomyDebuggerShared::GetDebugWorld();
		UArcGameDirectorSubsystem* Director = World ? World->GetSubsystem<UArcGameDirectorSubsystem>() : nullptr;
		UArcMassEntityConfigAsset* Config = CachedEntityConfigs.IsValidIndex(SelectedEntityConfigIdx)
			? Cast<UArcMassEntityConfigAsset>(CachedEntityConfigs[SelectedEntityConfigIdx].GetAsset())
			: nullptr;

		if (!World || !Director)
		{
			LastSpawnMessage = TEXT("No director subsystem in this world");
		}
		else if (!Config)
		{
			LastSpawnMessage = TEXT("Pick an entity config first");
		}
		else if (!CachedSettlements.IsValidIndex(SelectedSettlementIdx))
		{
			LastSpawnMessage = TEXT("Pick a settlement");
		}
		else
		{
			const FSettlementEntry& Settlement = CachedSettlements[SelectedSettlementIdx];
			const int32 SpawnedCount = Director->SpawnPopulation(Settlement.Entity, Config, SpawnUnassignedCount);
			LastSpawnMessage = FString::Printf(TEXT("Spawned %d unassigned into %s"), SpawnedCount, *Settlement.Name);
			RefreshAll();
		}
	}
	ImGui::EndDisabled();

	if (CachedSettlements.IsEmpty())
	{
		ImGui::TextDisabled("Place a settlement center first");
	}
	else if (!CachedSettlements.IsValidIndex(SelectedSettlementIdx))
	{
		ImGui::TextDisabled("Pick a settlement");
	}

	if (ImGui::Button("Spawn body"))
	{
		LastSpawnMessage.Reset();

		UWorld* World = ArcEconomyDebuggerShared::GetDebugWorld();
		FMassEntityManager* Manager = ArcEconomyDebuggerShared::GetEntityManager();
		UArcMassSpawnerSubsystem* Spawner = World ? World->GetSubsystem<UArcMassSpawnerSubsystem>() : nullptr;
		UArcMassEntityConfigAsset* Config = CachedEntityConfigs.IsValidIndex(SelectedEntityConfigIdx)
			? Cast<UArcMassEntityConfigAsset>(CachedEntityConfigs[SelectedEntityConfigIdx].GetAsset())
			: nullptr;

		if (!Spawner || !Manager)
		{
			LastSpawnMessage = TEXT("No spawner subsystem in this world");
		}
		else if (!Config)
		{
			LastSpawnMessage = TEXT("Pick an entity config first");
		}
		else if (!CachedBuildings.IsValidIndex(SpawnBuildingIdx))
		{
			LastSpawnMessage = TEXT("Pick a target building first");
		}
		else
		{
			static const EArcNPCEmploymentKind KindToEmployment[] = {
				EArcNPCEmploymentKind::Work,
				EArcNPCEmploymentKind::Gather,
				EArcNPCEmploymentKind::Carrier
			};

			const FBuildingEntry& Building = CachedBuildings[SpawnBuildingIdx];
			const FMassEntityHandle Spawned = Spawner->SpawnEntity(Config, FTransform(Building.Location));
			if (!Spawned.IsValid())
			{
				LastSpawnMessage = FString::Printf(TEXT("SpawnEntity returned an invalid handle\n%s"),
					*ArcEconomyDebuggerSpawnDiag::DescribeConfig(*Config, *World));
			}
			else
			{
				const EArcNPCEmploymentKind EmploymentKind = KindToEmployment[SpawnBodyKindIdx];
				FArcBodyFragment* Body = Manager->GetFragmentDataPtr<FArcBodyFragment>(Spawned);
				FArcSettlementMembershipFragment* Membership =
					Manager->GetFragmentDataPtr<FArcSettlementMembershipFragment>(Spawned);
				FArcWorkAssignmentFragment* Work = Manager->GetFragmentDataPtr<FArcWorkAssignmentFragment>(Spawned);
				FArcGatherAssignmentFragment* Gather = Manager->GetFragmentDataPtr<FArcGatherAssignmentFragment>(Spawned);
				FArcCarrierAssignmentFragment* Carrier = Manager->GetFragmentDataPtr<FArcCarrierAssignmentFragment>(Spawned);
				if (Body && Membership && Work && Gather && Carrier)
				{
					*Work = FArcWorkAssignmentFragment();
					*Gather = FArcGatherAssignmentFragment();
					*Carrier = FArcCarrierAssignmentFragment();
					TArray<ArcWorldSim::FSettlementBound> CentersView;
					CentersView.Reserve(CachedSettlements.Num());
					for (const FSettlementEntry& Settlement : CachedSettlements)
					{
						ArcWorldSim::FSettlementBound& Bound = CentersView.AddDefaulted_GetRef();
						Bound.Entity = Settlement.Entity;
						Bound.Transform = Settlement.Transform;
						Bound.Extent = Settlement.Extent;
					}
					if (CachedSettlements.IsValidIndex(SelectedSettlementIdx))
					{
						Membership->Settlement = CachedSettlements[SelectedSettlementIdx].Entity;
					}
					else
					{
						Membership->Settlement = ArcWorldSim::FindSettlementAt(Building.Location, CentersView);
					}
					Body->State = EArcBodyState::Idle;
					switch (EmploymentKind)
					{
					case EArcNPCEmploymentKind::Work:
						Work->Workplace = Building.Entity;
						LastSpawnMessage = FString::Printf(TEXT("Worker %s -> %s"), *Spawned.DebugGetDescription(), *Building.Name);
						break;
					case EArcNPCEmploymentKind::Gather:
						Gather->Workplace = Building.Entity;
						LastSpawnMessage = FString::Printf(TEXT("Gatherer %s -> %s"), *Spawned.DebugGetDescription(), *Building.Name);
						break;
					default:
						Carrier->bAssigned = true;
						LastSpawnMessage = FString::Printf(TEXT("Carrier %s idle"), *Spawned.DebugGetDescription());
						break;
					}
				}
				else
				{
					Spawner->DestroyEntities(TConstArrayView<FMassEntityHandle>(&Spawned, 1));
					LastSpawnMessage = FString::Printf(TEXT("Config is missing UArcBodyTrait fragments\n%s"),
						*ArcEconomyDebuggerSpawnDiag::DescribeConfig(*Config, *World));
				}
			}

			RefreshAll();
		}
	}

	ImGui::SameLine();
	if (ImGui::Button("Diagnose config"))
	{
		LastSpawnMessage.Reset();

		UWorld* World = ArcEconomyDebuggerShared::GetDebugWorld();
		UArcMassEntityConfigAsset* Config = CachedEntityConfigs.IsValidIndex(SelectedEntityConfigIdx)
			? Cast<UArcMassEntityConfigAsset>(CachedEntityConfigs[SelectedEntityConfigIdx].GetAsset())
			: nullptr;

		if (!World)
		{
			LastSpawnMessage = TEXT("No world to diagnose against");
		}
		else if (!Config)
		{
			LastSpawnMessage = TEXT("Pick an entity config first");
		}
		else
		{
			const bool bValid = Config->GetMutableConfig().ValidateEntityTemplate(*World);
			LastSpawnMessage = FString::Printf(TEXT("ValidateEntityTemplate: %s (see LogMass for details)\n%s"),
				bValid ? TEXT("ok") : TEXT("FAILED"),
				*ArcEconomyDebuggerSpawnDiag::DescribeConfig(*Config, *World));
		}
	}

	if (!LastSpawnMessage.IsEmpty())
	{
		if (ImGui::BeginChild("EconomySpawnMessage", ImVec2(0.0f, 220.0f), ImGuiChildFlags_Borders))
		{
			ImGui::TextUnformatted(TCHAR_TO_ANSI(*LastSpawnMessage));
		}
		ImGui::EndChild();
	}
#endif
}

void FArcEconomyDebugger::EnsureDrawActor(UWorld* World)
{
	if (DrawActor.IsValid())
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags = RF_Transient;
	AActor* NewActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
	if (!NewActor)
	{
		return;
	}

#if WITH_EDITOR
	NewActor->SetActorLabel(TEXT("EconomyDebuggerDraw"));
#endif

	UArcEconomyDebuggerDrawComponent* NewComponent = NewObject<UArcEconomyDebuggerDrawComponent>(NewActor, UArcEconomyDebuggerDrawComponent::StaticClass());
	NewComponent->RegisterComponent();
	NewActor->AddInstanceComponent(NewComponent);

	DrawActor = NewActor;
	DrawComponent = NewComponent;
}

void FArcEconomyDebugger::DestroyDrawActor()
{
	if (DrawActor.IsValid())
	{
		DrawActor->Destroy();
	}
	DrawActor.Reset();
	DrawComponent.Reset();
}

void FArcEconomyDebugger::UpdateDrawComponent()
{
#if WITH_MASSENTITY_DEBUG
	UWorld* World = ArcEconomyDebuggerShared::GetDebugWorld();
	if (!World)
	{
		return;
	}

	EnsureDrawActor(World);
	if (!DrawComponent.IsValid())
	{
		return;
	}

	FArcEconomyDebugDrawData Data;
	Data.bDrawLabels = bDrawLabels;

	if (bDrawBuildings)
	{
		for (int32 Index = 0; Index < CachedBuildings.Num(); ++Index)
		{
			const FBuildingEntry& Building = CachedBuildings[Index];
			FArcEconomyDebugDrawData::FBuildingMarker& Marker = Data.BuildingMarkers.AddDefaulted_GetRef();
			Marker.Location = Building.Location;
			Marker.Label = Building.Name;
			Marker.bSelected = (Index == SelectedBuildingIdx);
		}
	}

	FMassEntityManager* Manager = ArcEconomyDebuggerShared::GetEntityManager();

	if (bDrawNPCs && Manager)
	{
		for (int32 Index = 0; Index < CachedNPCs.Num(); ++Index)
		{
			const FNPCEntry& NPC = CachedNPCs[Index];
			if (!Manager->IsEntityActive(NPC.Entity))
			{
				continue;
			}

			const FTransformFragment* Transform = Manager->GetFragmentDataPtr<FTransformFragment>(NPC.Entity);
			if (!Transform)
			{
				continue;
			}

			FArcEconomyDebugDrawData::FNPCMarker& Marker = Data.NPCMarkers.AddDefaulted_GetRef();
			Marker.Location = Transform->GetTransform().GetLocation();
			Marker.bSelected = (Index == SelectedNPCIdx);
			switch (NPC.Kind)
			{
			case EArcNPCEmploymentKind::Work:   Marker.Color = FColor(50, 200, 50); break;
			case EArcNPCEmploymentKind::Gather: Marker.Color = FColor(100, 200, 255); break;
			case EArcNPCEmploymentKind::None:   Marker.Color = FColor(140, 140, 140); break;
			default:                     Marker.Color = FColor(255, 210, 60); break;
			}
		}
	}

	if (bDrawLinks)
	{
		for (const FNPCEntry& NPC : CachedNPCs)
		{
			if (!NPC.bHasTransportJob)
			{
				continue;
			}

			const int32 SourceIdx = FindBuildingIndex(NPC.Source);
			const int32 SinkIdx = FindBuildingIndex(NPC.Sink);
			if (!CachedBuildings.IsValidIndex(SourceIdx) || !CachedBuildings.IsValidIndex(SinkIdx))
			{
				continue;
			}

			FArcEconomyDebugDrawData::FLink& Link = Data.Links.AddDefaulted_GetRef();
			Link.Start = CachedBuildings[SourceIdx].Location;
			Link.End = CachedBuildings[SinkIdx].Location;
			Link.Color = FColor(255, 210, 60);
		}
	}

	const FColor SettlementBoundsColor(0, 220, 220);
	for (const FSettlementEntry& Settlement : CachedSettlements)
	{
		const FVector Extent = Settlement.Extent;
		const FVector Corners[8] = {
			Settlement.Transform.TransformPosition(FVector(-Extent.X, -Extent.Y, -Extent.Z)),
			Settlement.Transform.TransformPosition(FVector(+Extent.X, -Extent.Y, -Extent.Z)),
			Settlement.Transform.TransformPosition(FVector(+Extent.X, +Extent.Y, -Extent.Z)),
			Settlement.Transform.TransformPosition(FVector(-Extent.X, +Extent.Y, -Extent.Z)),
			Settlement.Transform.TransformPosition(FVector(-Extent.X, -Extent.Y, +Extent.Z)),
			Settlement.Transform.TransformPosition(FVector(+Extent.X, -Extent.Y, +Extent.Z)),
			Settlement.Transform.TransformPosition(FVector(+Extent.X, +Extent.Y, +Extent.Z)),
			Settlement.Transform.TransformPosition(FVector(-Extent.X, +Extent.Y, +Extent.Z)),
		};
		const int32 EdgePairs[12][2] = {
			{0, 1}, {1, 2}, {2, 3}, {3, 0},
			{4, 5}, {5, 6}, {6, 7}, {7, 4},
			{0, 4}, {1, 5}, {2, 6}, {3, 7},
		};
		for (int32 EdgeIdx = 0; EdgeIdx < 12; ++EdgeIdx)
		{
			FArcEconomyDebugDrawData::FLink& Link = Data.Links.AddDefaulted_GetRef();
			Link.Start = Corners[EdgePairs[EdgeIdx][0]];
			Link.End = Corners[EdgePairs[EdgeIdx][1]];
			Link.Color = SettlementBoundsColor;
		}
	}

	DrawComponent->UpdateData(Data);
#endif
}
