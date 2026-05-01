// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEconomyDebugger.h"

#include "imgui.h"
#include "ArcEconomyDebuggerDrawComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "MassEntitySubsystem.h"
#include "MassDebugger.h"
#include "MassActorSubsystem.h"
#include "Mass/EntityFragments.h"
#include "ArcSettlementSubsystem.h"
#include "Mass/ArcEconomyFragments.h"
#include "Data/ArcGovernorDataAsset.h"
#include "SmartObjectSubsystem.h"
#include "ArcAreaSubsystem.h"
#include "Mass/ArcAreaFragments.h"
#include "ArcMass/SmartObject/ArcMassSmartObjectFragments.h"
#include "Fragments/ArcSettlementNeedFragments.h"
#include "Fragments/ArcNeedFragment.h"
#include "ArcMass/Spatial/ArcMassSpatialHashSubsystem.h"

namespace Arcx::GameplayDebugger::Economy
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
		UMassEntitySubsystem* MassSub = World->GetSubsystem<UMassEntitySubsystem>();
		if (!MassSub)
		{
			return nullptr;
		}
		return &MassSub->GetMutableEntityManager();
	}

	UArcSettlementSubsystem* GetSettlementSubsystem()
	{
		UWorld* World = GetDebugWorld();
		if (!World)
		{
			return nullptr;
		}
		return World->GetSubsystem<UArcSettlementSubsystem>();
	}

	// Colors (extern const — referenced by other TUs via extern declarations)
	extern const ImVec4 PlayerColor(0.3f, 0.6f, 1.0f, 1.0f);
	extern const ImVec4 AIColor(1.0f, 0.6f, 0.2f, 1.0f);
	extern const ImVec4 DimColor(0.5f, 0.5f, 0.5f, 1.0f);
	extern const ImVec4 HeaderColor(0.4f, 1.0f, 0.4f, 1.0f);
} // namespace Arcx::GameplayDebugger::Economy

void FArcEconomyDebugger::Initialize()
{
	SelectedSettlementIdx = INDEX_NONE;
	SelectedBuildingIdx = INDEX_NONE;
	SettlementFilterBuf[0] = '\0';
	CachedSettlements.Reset();
	CachedMarketEntries.Reset();
	CachedKnowledgeEntries.Reset();
	CachedBuildings.Reset();
	CachedNPCs.Reset();
	CachedSOSlots.Reset();
	CachedDemandGraph.Reset();
	NodePositions.Reset();
	bDemandGraphDirty = true;
	HoveredNodeIdx = INDEX_NONE;
	SelectedGraphNodeIdx = INDEX_NONE;
	CachedNeeds.Reset();
	LastRefreshTime = 0.f;
	RefreshSettlementList();
}

void FArcEconomyDebugger::Uninitialize()
{
	CachedSettlements.Reset();
	CachedMarketEntries.Reset();
	CachedKnowledgeEntries.Reset();
	CachedBuildings.Reset();
	CachedNPCs.Reset();
	CachedSOSlots.Reset();
	CachedDemandGraph.Reset();
	NodePositions.Reset();
	CachedNeeds.Reset();
	SelectedSettlementIdx = INDEX_NONE;
	SelectedBuildingIdx = INDEX_NONE;
	DestroyDrawActor();
}

void FArcEconomyDebugger::RefreshSettlementList()
{
#if WITH_MASSENTITY_DEBUG
	CachedSettlements.Reset();

	FMassEntityManager* Manager = Arcx::GameplayDebugger::Economy::GetEntityManager();
	if (!Manager)
	{
		return;
	}

	const UScriptStruct* SettlementType = FArcSettlementFragment::StaticStruct();
	const UScriptStruct* WorkforceType = FArcSettlementWorkforceFragment::StaticStruct();

	TArray<FMassArchetypeHandle> Archetypes = FMassDebugger::GetAllArchetypes(*Manager);

	for (const FMassArchetypeHandle& Archetype : Archetypes)
	{
		FMassArchetypeCompositionDescriptor Composition = FMassDebugger::GetArchetypeComposition(Archetype);
		if (!Composition.Contains(SettlementType))
		{
			continue;
		}

		const bool bHasWorkforce = Composition.Contains(WorkforceType);

		TArray<FMassEntityHandle> Entities = FMassDebugger::GetEntitiesOfArchetype(Archetype);
		for (const FMassEntityHandle& Entity : Entities)
		{
			FStructView SettlementView = Manager->GetFragmentDataStruct(Entity, SettlementType);
			if (!SettlementView.IsValid())
			{
				continue;
			}

			const FArcSettlementFragment& Settlement = SettlementView.Get<FArcSettlementFragment>();

			FSettlementEntry& Entry = CachedSettlements.AddDefaulted_GetRef();
			Entry.Entity = Entity;
			Entry.Name = Settlement.SettlementName.ToString();
			Entry.bPlayerOwned = Settlement.bPlayerOwned;

			if (bHasWorkforce)
			{
				FStructView WorkforceView = Manager->GetFragmentDataStruct(Entity, WorkforceType);
				if (WorkforceView.IsValid())
				{
					const FArcSettlementWorkforceFragment& Workforce = WorkforceView.Get<FArcSettlementWorkforceFragment>();
					Entry.WorkerCount = Workforce.WorkerCount;
					Entry.TransporterCount = Workforce.TransporterCount;
					Entry.GathererCount = Workforce.GathererCount;
					Entry.CaravanCount = Workforce.CaravanCount;
					Entry.IdleCount = Workforce.IdleCount;
				}
			}
		}
	}
#endif
}

void FArcEconomyDebugger::Draw()
{
#if WITH_MASSENTITY_DEBUG
	ImGui::SetNextWindowSize(ImVec2(1100, 600), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Economy Debugger", &bShow))
	{
		ImGui::End();
		return;
	}

	FMassEntityManager* Manager = Arcx::GameplayDebugger::Economy::GetEntityManager();
	if (!Manager)
	{
		ImGui::TextDisabled("No MassEntitySubsystem available");
		ImGui::End();
		return;
	}

	// Auto-refresh
	UWorld* World = Arcx::GameplayDebugger::Economy::GetDebugWorld();
	if (World)
	{
		float CurrentTime = World->GetTimeSeconds();
		if (CurrentTime - LastRefreshTime > 1.0f)
		{
			LastRefreshTime = CurrentTime;
			RefreshSettlementList();
			if (SelectedSettlementIdx != INDEX_NONE && CachedSettlements.IsValidIndex(SelectedSettlementIdx))
			{
				RefreshSelectedSettlementData();
			}
		}
	}

	// Draw toggles toolbar
	ImGui::Checkbox("Radius", &bDrawRadius);
	ImGui::SameLine();
	ImGui::Checkbox("Buildings", &bDrawBuildings);
	ImGui::SameLine();
	ImGui::Checkbox("NPCs", &bDrawNPCs);
	ImGui::SameLine();
	ImGui::Checkbox("Links", &bDrawLinks);
	ImGui::SameLine();
	ImGui::Checkbox("Labels", &bDrawLabels);
	ImGui::SameLine();
	ImGui::Checkbox("SO Slots", &bDrawSOSlots);
	ImGui::SameLine();
	if (ImGui::Button("Refresh"))
	{
		RefreshSettlementList();
		if (SelectedSettlementIdx != INDEX_NONE)
		{
			RefreshSelectedSettlementData();
		}
	}
	ImGui::SameLine();
	ImGui::Text("Settlements: %d", CachedSettlements.Num());

	ImGui::Separator();

	// Two-pane layout
	float PanelWidth = ImGui::GetContentRegionAvail().x;
	float LeftPanelWidth = PanelWidth * 0.25f;

	if (ImGui::BeginChild("SettlementListPanel", ImVec2(LeftPanelWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
	{
		DrawSettlementListPanel();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	if (ImGui::BeginChild("SettlementDetailPanel", ImVec2(0, 0), ImGuiChildFlags_Borders))
	{
		DrawDetailPanel();
	}
	ImGui::EndChild();

	// Update viewport drawing
	UpdateDrawComponent();

	ImGui::End();
#endif
}

void FArcEconomyDebugger::DrawSettlementListPanel()
{
#if WITH_MASSENTITY_DEBUG
	ImGui::Text("Settlements");
	ImGui::Separator();

	ImGui::InputText("Filter", SettlementFilterBuf, IM_ARRAYSIZE(SettlementFilterBuf));

	FString Filter = FString(ANSI_TO_TCHAR(SettlementFilterBuf)).ToLower();

	if (ImGui::BeginChild("SettlementScroll", ImVec2(0, 0)))
	{
		for (int32 i = 0; i < CachedSettlements.Num(); ++i)
		{
			const FSettlementEntry& Entry = CachedSettlements[i];

			if (!Filter.IsEmpty() && !Entry.Name.ToLower().Contains(Filter))
			{
				continue;
			}

			FString DisplayLabel = FString::Printf(TEXT("%s %s W:%d T:%d G:%d C:%d I:%d"),
				*Entry.Name,
				Entry.bPlayerOwned ? TEXT("[Player]") : TEXT("[AI]"),
				Entry.WorkerCount, Entry.TransporterCount,
				Entry.GathererCount, Entry.CaravanCount, Entry.IdleCount);

			const bool bSelected = (SelectedSettlementIdx == i);
			if (ImGui::Selectable(TCHAR_TO_ANSI(*DisplayLabel), bSelected))
			{
				SelectedSettlementIdx = i;
				SelectedBuildingIdx = INDEX_NONE;
				RefreshSelectedSettlementData();
			}
		}
	}
	ImGui::EndChild();
#endif
}

void FArcEconomyDebugger::DrawDetailPanel()
{
#if WITH_MASSENTITY_DEBUG
	if (SelectedSettlementIdx == INDEX_NONE || !CachedSettlements.IsValidIndex(SelectedSettlementIdx))
	{
		ImGui::TextDisabled("Select a settlement from the list");
		return;
	}

	FMassEntityManager* Manager = Arcx::GameplayDebugger::Economy::GetEntityManager();
	if (!Manager)
	{
		return;
	}

	const FSettlementEntry& Entry = CachedSettlements[SelectedSettlementIdx];
	if (!Manager->IsEntityActive(Entry.Entity))
	{
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Settlement entity is no longer active");
		return;
	}

	ImVec4 BadgeColor = Entry.bPlayerOwned
		? Arcx::GameplayDebugger::Economy::PlayerColor
		: Arcx::GameplayDebugger::Economy::AIColor;
	ImGui::TextColored(BadgeColor, "%s", TCHAR_TO_ANSI(*Entry.Name));
	ImGui::SameLine();
	ImGui::TextColored(BadgeColor, Entry.bPlayerOwned ? "[Player]" : "[AI]");
	ImGui::Separator();

	if (ImGui::BeginTabBar("SettlementTabs"))
	{
		if (ImGui::BeginTabItem("Overview"))
		{
			DrawOverviewTab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Market"))
		{
			DrawMarketTab();
			ImGui::EndTabItem();
		}
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
		if (ImGui::BeginTabItem("Demand Graph"))
		{
			DrawDemandGraphTab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Needs"))
		{
			DrawNeedsTab();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
#endif
}

void FArcEconomyDebugger::RefreshSelectedSettlementData()
{
	RefreshMarketData();
	RefreshKnowledgeEntries();
	RefreshBuildingList();
	RefreshNPCList();
	RefreshSmartObjectSlots();
	RefreshNeedsData();
}

void FArcEconomyDebugger::DrawOverviewTab()
{
#if WITH_MASSENTITY_DEBUG
	FMassEntityManager* Manager = Arcx::GameplayDebugger::Economy::GetEntityManager();
	if (!Manager)
	{
		return;
	}

	const FSettlementEntry& Entry = CachedSettlements[SelectedSettlementIdx];
	const FMassEntityHandle Entity = Entry.Entity;

	// Settlement fragment
	FStructView SettlementView = Manager->GetFragmentDataStruct(Entity, FArcSettlementFragment::StaticStruct());
	if (SettlementView.IsValid())
	{
		const FArcSettlementFragment& Settlement = SettlementView.Get<FArcSettlementFragment>();
		ImGui::SeparatorText("Settlement");
		ImGui::Text("Radius: %.0f", Settlement.SettlementRadius);
	}

	// Workforce
	ImGui::SeparatorText("Workforce");
	ImGui::Text("Workers: %d  Transporters: %d  Gatherers: %d  Caravans: %d  Idle: %d",
		Entry.WorkerCount, Entry.TransporterCount, Entry.GathererCount, Entry.CaravanCount, Entry.IdleCount);

	int32 Total = Entry.WorkerCount + Entry.TransporterCount + Entry.GathererCount + Entry.CaravanCount + Entry.IdleCount;
	ImGui::Text("Total NPCs: %d", Total);

	// Governor config
	FStructView GovernorView = Manager->GetFragmentDataStruct(Entity, FArcGovernorFragment::StaticStruct());
	if (GovernorView.IsValid())
	{
		const FArcGovernorFragment& Governor = GovernorView.Get<FArcGovernorFragment>();
		UArcGovernorDataAsset* Config = Governor.GovernorConfig;

		ImGui::SeparatorText("Governor Config");

		if (!Config)
		{
			ImGui::TextDisabled("No governor config assigned");
		}
		else
		{
			ImGui::InputFloat("Worker/Transporter Ratio", &Config->TargetWorkerTransporterRatio, 0.1f, 1.0f, "%.1f");
			ImGui::InputInt("Output Backlog Threshold", &Config->OutputBacklogThreshold);
			ImGui::InputFloat("Trade Price Differential", &Config->TradePriceDifferentialThreshold, 0.1f, 0.5f, "%.1f");
			ImGui::InputFloat("Trade Eval Interval (s)", &Config->TradeEvalInterval, 0.1f, 0.5f, "%.1f");
			ImGui::InputInt("Default Output Buffer", &Config->DefaultOutputBufferSize);
			ImGui::InputInt("Default Storage Cap", &Config->DefaultTotalStorageCap);

			const FString ConfigName = Config->GetFName().ToString();
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Config asset: %s (shared)", TCHAR_TO_ANSI(*ConfigName));
		}
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
	UWorld* World = Arcx::GameplayDebugger::Economy::GetDebugWorld();
	if (!World)
	{
		return;
	}

	EnsureDrawActor(World);
	if (!DrawComponent.IsValid())
	{
		return;
	}

	FMassEntityManager* Manager = Arcx::GameplayDebugger::Economy::GetEntityManager();
	if (!Manager)
	{
		return;
	}

	FArcEconomyDebugDrawData Data;
	Data.bDrawLabels = bDrawLabels;

	// Populate all settlement markers
	if (bDrawRadius)
	{
		for (int32 i = 0; i < CachedSettlements.Num(); ++i)
		{
			const FSettlementEntry& Entry = CachedSettlements[i];
			if (!Manager->IsEntityActive(Entry.Entity))
			{
				continue;
			}

			FStructView TransformView = Manager->GetFragmentDataStruct(Entry.Entity, FTransformFragment::StaticStruct());
			if (!TransformView.IsValid())
			{
				continue;
			}

			const FTransformFragment& Transform = TransformView.Get<FTransformFragment>();
			FStructView SettlementView = Manager->GetFragmentDataStruct(Entry.Entity, FArcSettlementFragment::StaticStruct());
			if (!SettlementView.IsValid())
			{
				continue;
			}

			const FArcSettlementFragment& Settlement = SettlementView.Get<FArcSettlementFragment>();

			FArcEconomyDebugDrawData::FSettlementMarker& Marker = Data.SettlementMarkers.AddDefaulted_GetRef();
			Marker.Location = Transform.GetTransform().GetLocation();
			Marker.Radius = Settlement.SettlementRadius;
			Marker.Label = Entry.Name;
			Marker.Color = Entry.bPlayerOwned ? FColor(77, 153, 255) : FColor(255, 153, 51);
			Marker.bSelected = (i == SelectedSettlementIdx);
		}
	}

	// Populate building markers for selected settlement
	if (bDrawBuildings && SelectedSettlementIdx != INDEX_NONE && CachedSettlements.IsValidIndex(SelectedSettlementIdx))
	{
		for (const FBuildingEntry& Building : CachedBuildings)
		{
			FArcEconomyDebugDrawData::FBuildingMarker& Marker = Data.BuildingMarkers.AddDefaulted_GetRef();
			Marker.Location = Building.Location;
			Marker.Label = Building.DefinitionName;
		}
	}

	// Populate SO slot markers — query live from SmartObjectSubsystem for all buildings
	if (bDrawSOSlots && SelectedSettlementIdx != INDEX_NONE && CachedSettlements.IsValidIndex(SelectedSettlementIdx))
	{
		USmartObjectSubsystem* SOSub = World->GetSubsystem<USmartObjectSubsystem>();
		UArcAreaSubsystem* AreaSub = World->GetSubsystem<UArcAreaSubsystem>();

		if (SOSub && AreaSub)
		{
			for (const FBuildingEntry& Building : CachedBuildings)
			{
				if (!Manager->IsEntityActive(Building.Entity))
				{
					continue;
				}

				// Resolve SmartObjectHandle via FArcAreaFragment -> FArcAreaData
				FSmartObjectHandle SOHandle;
				FStructView AreaView = Manager->GetFragmentDataStruct(Building.Entity, FArcAreaFragment::StaticStruct());
				if (AreaView.IsValid())
				{
					const FArcAreaFragment& AreaFrag = AreaView.Get<FArcAreaFragment>();
					const FArcAreaData* AreaData = AreaSub->GetAreaData(AreaFrag.AreaHandle);
					if (AreaData)
					{
						SOHandle = AreaData->SmartObjectHandle;
					}
				}

				// Fallback: FArcSmartObjectOwnerFragment
				if (!SOHandle.IsValid())
				{
					FStructView OwnerView = Manager->GetFragmentDataStruct(Building.Entity, FArcSmartObjectOwnerFragment::StaticStruct());
					if (OwnerView.IsValid())
					{
						SOHandle = OwnerView.Get<FArcSmartObjectOwnerFragment>().SmartObjectHandle;
					}
				}

				if (!SOHandle.IsValid())
				{
					continue;
				}

				TArray<FSmartObjectSlotHandle> SOSlots;
				SOSub->GetAllSlots(SOHandle, SOSlots);

				for (int32 i = 0; i < SOSlots.Num(); ++i)
				{
					TOptional<FTransform> SlotTransform = SOSub->GetSlotTransform(SOSlots[i]);
					if (!SlotTransform.IsSet())
					{
						continue;
					}

					const ESmartObjectSlotState SOState = SOSub->GetSlotState(SOSlots[i]);

					FArcEconomyDebugDrawData::FSOSlotMarker& Marker = Data.SOSlotMarkers.AddDefaulted_GetRef();
					Marker.Location = SlotTransform.GetValue().GetLocation();

					switch (SOState)
					{
					case ESmartObjectSlotState::Free:     Marker.Color = FColor(50, 200, 50); break;
					case ESmartObjectSlotState::Claimed:  Marker.Color = FColor(200, 150, 50); break;
					case ESmartObjectSlotState::Occupied: Marker.Color = FColor(50, 150, 255); break;
					default:                              Marker.Color = FColor(150, 50, 50); break;
					}

					Marker.Label = FString::Printf(TEXT("SO%d [%s]"), i,
						SOState == ESmartObjectSlotState::Free     ? TEXT("Free") :
						SOState == ESmartObjectSlotState::Claimed  ? TEXT("Claimed") :
						SOState == ESmartObjectSlotState::Occupied ? TEXT("Occupied") : TEXT("Invalid"));
				}
			}
		}
	}

	// Populate NPC markers and links for selected settlement
	if ((bDrawNPCs || bDrawLinks) && SelectedSettlementIdx != INDEX_NONE && CachedSettlements.IsValidIndex(SelectedSettlementIdx))
	{
		for (const FNPCEntry& NPC : CachedNPCs)
		{
			if (!Manager->IsEntityActive(NPC.Entity))
			{
				continue;
			}

			FStructView TransformView = Manager->GetFragmentDataStruct(NPC.Entity, FTransformFragment::StaticStruct());
			if (!TransformView.IsValid())
			{
				continue;
			}

			FVector NPCLocation = TransformView.Get<FTransformFragment>().GetTransform().GetLocation();

			if (bDrawNPCs)
			{
				FArcEconomyDebugDrawData::FNPCMarker& Marker = Data.NPCMarkers.AddDefaulted_GetRef();
				Marker.Location = NPCLocation;
				// Color by role
				switch (static_cast<EArcEconomyNPCRole>(NPC.Role))
				{
				case EArcEconomyNPCRole::Worker:      Marker.Color = FColor::Green; break;
				case EArcEconomyNPCRole::Transporter:  Marker.Color = FColor::Yellow; break;
				case EArcEconomyNPCRole::Gatherer:     Marker.Color = FColor(100, 200, 255); break;
				default:                               Marker.Color = FColor::Red; break;
				}
			}

			if (bDrawLinks)
			{
				// Worker -> Building link
				if (static_cast<EArcEconomyNPCRole>(NPC.Role) == EArcEconomyNPCRole::Worker
					&& Manager->IsEntityActive(NPC.AssignedBuilding))
				{
					FStructView BuildingTransform = Manager->GetFragmentDataStruct(NPC.AssignedBuilding, FTransformFragment::StaticStruct());
					if (BuildingTransform.IsValid())
					{
						FArcEconomyDebugDrawData::FLink& Link = Data.Links.AddDefaulted_GetRef();
						Link.Start = NPCLocation;
						Link.End = BuildingTransform.Get<FTransformFragment>().GetTransform().GetLocation();
						Link.Color = FColor::Green;
					}
				}

				if (static_cast<EArcEconomyNPCRole>(NPC.Role) == EArcEconomyNPCRole::Gatherer)
				{
					if (Manager->IsEntityActive(NPC.AssignedBuilding))
					{
						FStructView BuildingTransform = Manager->GetFragmentDataStruct(NPC.AssignedBuilding, FTransformFragment::StaticStruct());
						if (BuildingTransform.IsValid())
						{
							FArcEconomyDebugDrawData::FLink& Link = Data.Links.AddDefaulted_GetRef();
							Link.Start = NPCLocation;
							Link.End = BuildingTransform.Get<FTransformFragment>().GetTransform().GetLocation();
							Link.Color = FColor(100, 200, 255);
						}
					}
					if (Manager->IsEntityActive(NPC.GathererTargetResource))
					{
						const FTransformFragment* ResTransform = Manager->GetFragmentDataPtr<FTransformFragment>(NPC.GathererTargetResource);
						if (ResTransform)
						{
							FArcEconomyDebugDrawData::FLink& Link = Data.Links.AddDefaulted_GetRef();
							Link.Start = NPCLocation;
							Link.End = ResTransform->GetTransform().GetLocation();
							Link.Color = FColor(255, 165, 0);
						}
					}
				}
			}
		}
	}

	// Player location
	APlayerController* PC = World->GetFirstPlayerController();
	if (PC && PC->GetPawn())
	{
		Data.PlayerLocation = PC->GetPawn()->GetActorLocation();
	}

	// Selected building highlight
	if (SelectedBuildingIdx != INDEX_NONE && CachedBuildings.IsValidIndex(SelectedBuildingIdx))
	{
		Data.bHasSelectedBuilding = true;
		Data.SelectedBuildingLocation = CachedBuildings[SelectedBuildingIdx].Location;

		const FArcBuildingEconomyConfig* SelEconConfig = Manager->GetSharedFragmentDataPtr<FArcBuildingEconomyConfig>(CachedBuildings[SelectedBuildingIdx].Entity);
		if (SelEconConfig && SelEconConfig->IsGatheringBuilding())
		{
			Data.SelectedBuildingSearchRadius = SelEconConfig->GatherSearchRadius;

			UArcMassSpatialHashSubsystem* SpatialSub = World->GetSubsystem<UArcMassSpatialHashSubsystem>();
			if (SpatialSub)
			{
				TSet<FMassEntityHandle> OccupiedResources;
				for (const FNPCEntry& NPC : CachedNPCs)
				{
					if (static_cast<EArcEconomyNPCRole>(NPC.Role) == EArcEconomyNPCRole::Gatherer
						&& NPC.GathererTargetResource.IsValid())
					{
						OccupiedResources.Add(NPC.GathererTargetResource);
					}
				}

				TArray<FArcMassEntityInfo> SpatialResults;
				for (const FGameplayTag& Tag : SelEconConfig->GatherSearchTags)
				{
					SpatialSub->QuerySphere(ArcSpatialHash::HashKey(Tag), Data.SelectedBuildingLocation, SelEconConfig->GatherSearchRadius, SpatialResults);
				}

				for (const FArcMassEntityInfo& Result : SpatialResults)
				{
					const bool bOccupied = OccupiedResources.Contains(Result.Entity);

					FArcEconomyDebugDrawData::FResourceMarker& Marker = Data.ResourceMarkers.AddDefaulted_GetRef();
					Marker.Location = Result.Location;
					Marker.Color = bOccupied ? FColor(255, 165, 0) : FColor(50, 200, 50);
					Marker.Label = FString::Printf(TEXT("Res %d%s"), Result.Entity.Index, bOccupied ? TEXT(" [Occupied]") : TEXT(""));

					FArcEconomyDebugDrawData::FLink& Link = Data.Links.AddDefaulted_GetRef();
					Link.Start = Data.SelectedBuildingLocation;
					Link.End = Result.Location;
					Link.Color = bOccupied ? FColor(255, 165, 0, 128) : FColor(50, 200, 50, 128);
				}
			}
		}
	}

	// Selected NPC highlight
	if (SelectedNPCIdx != INDEX_NONE && CachedNPCs.IsValidIndex(SelectedNPCIdx))
	{
		const FNPCEntry& SelectedNPC = CachedNPCs[SelectedNPCIdx];
		if (Manager->IsEntityActive(SelectedNPC.Entity))
		{
			FStructView NPCTransform = Manager->GetFragmentDataStruct(SelectedNPC.Entity, FTransformFragment::StaticStruct());
			if (NPCTransform.IsValid())
			{
				Data.bHasSelectedNPC = true;
				Data.SelectedNPCLocation = NPCTransform.Get<FTransformFragment>().GetTransform().GetLocation();
			}
		}
	}

	DrawComponent->UpdateData(Data);
#endif
}

void FArcEconomyDebugger::RefreshNeedsData()
{
#if WITH_MASSENTITY_DEBUG
	CachedNeeds.Reset();

	FMassEntityManager* Manager = Arcx::GameplayDebugger::Economy::GetEntityManager();
	if (!Manager || SelectedSettlementIdx == INDEX_NONE)
	{
		return;
	}

	const FSettlementEntry& Entry = CachedSettlements[SelectedSettlementIdx];
	if (!Manager->IsEntityActive(Entry.Entity))
	{
		return;
	}

	struct FNeedCheck
	{
		UScriptStruct* Struct;
		const TCHAR* Name;
	};

	const FNeedCheck NeedChecks[] = {
		{ FArcSettlementFoodNeed::StaticStruct(),       TEXT("Food") },
		{ FArcSettlementWaterNeed::StaticStruct(),      TEXT("Water") },
		{ FArcSettlementShelterNeed::StaticStruct(),    TEXT("Shelter") },
		{ FArcSettlementClothingNeed::StaticStruct(),   TEXT("Clothing") },
		{ FArcSettlementHealthcareNeed::StaticStruct(), TEXT("Healthcare") },
		{ FArcSettlementMoraleNeed::StaticStruct(),     TEXT("Morale") },
	};

	for (const FNeedCheck& Check : NeedChecks)
	{
		FStructView View = Manager->GetFragmentDataStruct(Entry.Entity, Check.Struct);
		if (View.IsValid())
		{
			const FArcNeedFragment& Need = View.Get<FArcNeedFragment>();
			FNeedEntry& NeedEntry = CachedNeeds.AddDefaulted_GetRef();
			NeedEntry.Name = Check.Name;
			NeedEntry.CurrentValue = Need.CurrentValue;
			NeedEntry.ChangeRate = Need.ChangeRate;
		}
	}
#endif
}

void FArcEconomyDebugger::DrawNeedsTab()
{
#if WITH_MASSENTITY_DEBUG
	if (CachedNeeds.Num() == 0)
	{
		ImGui::TextDisabled("No settlement needs configured");
		return;
	}

	ImGui::SeparatorText("Survival");

	for (int32 i = 0; i < CachedNeeds.Num(); ++i)
	{
		const FNeedEntry& Need = CachedNeeds[i];

		// Separator between tiers
		if (i == 3)
		{
			ImGui::SeparatorText("Comfort");
		}

		float Ratio = Need.CurrentValue / 100.0f;
		ImVec4 BarColor(Ratio, 1.0f - Ratio, 0.0f, 1.0f);

		ImGui::Text("%-12s", TCHAR_TO_ANSI(*Need.Name));
		ImGui::SameLine(120.0f);

		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, BarColor);
		FString Overlay = FString::Printf(TEXT("%.1f / 100"), Need.CurrentValue);
		ImGui::ProgressBar(Ratio, ImVec2(200.0f, 0.0f), TCHAR_TO_ANSI(*Overlay));
		ImGui::PopStyleColor();

		if (FMath::Abs(Need.ChangeRate) > UE_SMALL_NUMBER)
		{
			ImGui::SameLine();
			ImVec4 RateColor = Need.ChangeRate > 0.0f
				? ImVec4(1.0f, 0.3f, 0.3f, 1.0f)
				: ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
			ImGui::TextColored(RateColor, "%+.2f/s", Need.ChangeRate);
		}
	}
#endif
}
