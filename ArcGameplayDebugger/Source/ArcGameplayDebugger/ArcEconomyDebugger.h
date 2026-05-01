// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "SmartObjectTypes.h"
#include "Mass/ArcDemandGraph.h"

class UArcEconomyDebuggerDrawComponent;
class UArcItemDefinition;
class UArcRecipeDefinition;

class FArcEconomyDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	// --- Settlement list entry ---
	struct FSettlementEntry
	{
		FMassEntityHandle Entity;
		FString Name;
		bool bPlayerOwned = false;
		int32 WorkerCount = 0;
		int32 TransporterCount = 0;
		int32 GathererCount = 0;
		int32 CaravanCount = 0;
		int32 IdleCount = 0;
	};

	// --- Building list entry ---
	struct FBuildingEntry
	{
		FMassEntityHandle Entity;
		FString DefinitionName;
		FVector Location = FVector::ZeroVector;
		int32 SlotCount = 0;
	};

	// --- NPC list entry ---
	struct FNPCEntry
	{
		FMassEntityHandle Entity;
		FString Label;
		uint8 Role = 0; // EArcEconomyNPCRole
		FMassEntityHandle AssignedBuilding;
		uint8 TransporterState = 0; // EArcTransporterTaskState
		FString TargetItemName;
		int32 TargetQuantity = 0;
		FMassEntityHandle GathererTargetResource; // For gatherer role
		bool bCarryingResource = false;
	};

	// --- Market table entry ---
	struct FMarketEntry
	{
		FString ItemName;
		TObjectPtr<UArcItemDefinition> ItemDef = nullptr;
		float Price = 0.0f;
		float Supply = 0.0f;
		float Demand = 0.0f;
	};

	// --- Knowledge entry ---
	struct FKnowledgeEntry
	{
		FString Type; // "Demand", "Supply", "Market"
		FString ItemName;
		FString Detail;
	};

	// --- Smart Object slot entry ---
	struct FSOSlotEntry
	{
		FSmartObjectSlotHandle SlotHandle;
		FVector Location = FVector::ZeroVector;
		uint8 State = 0; // ESmartObjectSlotState
		FGameplayTagContainer ActivityTags;
		FGameplayTagContainer RuntimeTags;
		bool bEnabled = true;
	};

	// --- Need entry ---
	struct FNeedEntry
	{
		FString Name;
		float CurrentValue = 0.0f;
		float ChangeRate = 0.0f;
	};

	// --- Helpers (ArcEconomyDebugger.cpp) ---
	void RefreshSettlementList();
	void RefreshSelectedSettlementData();

	// --- Overview tab (ArcEconomyDebugger.cpp) ---
	void DrawSettlementListPanel();
	void DrawDetailPanel();
	void DrawOverviewTab();

	// --- Market tab (ArcEconomyDebuggerMarket.cpp) ---
	void DrawMarketTab();
	void RefreshMarketData();
	void RefreshKnowledgeEntries();

	// --- Buildings tab (ArcEconomyDebuggerBuildings.cpp) ---
	void DrawBuildingsTab();
	void RefreshBuildingList();
	void DrawBuildingDetail();
	void RefreshSmartObjectSlots();
	void DrawSmartObjectSlotSection();
	void DrawAreaSlotSection();

	// --- Workforce tab (ArcEconomyDebuggerWorkforce.cpp) ---
	void DrawWorkforceTab();
	void RefreshNPCList();
	void DrawNPCDetailPanel();
	void DrawNPCStateTree();
	void DrawNPCInventory();

	// --- Demand Graph tab (ArcEconomyDebuggerDemandGraph.cpp) ---
	void DrawDemandGraphTab();
	void RefreshDemandGraph();

	// --- Needs tab (ArcEconomyDebugger.cpp) ---
	void DrawNeedsTab();
	void RefreshNeedsData();

	// --- Draw component (ArcEconomyDebugger.cpp) ---
	void UpdateDrawComponent();
	void EnsureDrawActor(UWorld* World);
	void DestroyDrawActor();

	// --- State ---
	TArray<FSettlementEntry> CachedSettlements;
	int32 SelectedSettlementIdx = INDEX_NONE;
	char SettlementFilterBuf[256] = {};
	float LastRefreshTime = 0.f;

	// Overview state
	float CachedRadius = 0.f;
	float CachedGovernorWorkerTransporterRatio = 0.f;
	int32 CachedGovernorOutputBacklogThreshold = 0;
	float CachedGovernorTradePriceDifferentialThreshold = 0.f;

	// Market state
	TArray<FMarketEntry> CachedMarketEntries;
	TArray<FKnowledgeEntry> CachedKnowledgeEntries;
	float CachedMarketK = 0.f;

	// Buildings state
	TArray<FBuildingEntry> CachedBuildings;
	int32 SelectedBuildingIdx = INDEX_NONE;
	TArray<FSOSlotEntry> CachedSOSlots; // for selected building

	// Workforce state
	TArray<FNPCEntry> CachedNPCs;
	int32 SelectedNPCIdx = INDEX_NONE;

	// Demand graph state
	ArcDemandGraph::FDemandGraph CachedDemandGraph;
	TMap<int32, FVector2D> NodePositions;
	bool bDemandGraphDirty = true;
	int32 HoveredNodeIdx = INDEX_NONE;
	int32 SelectedGraphNodeIdx = INDEX_NONE;

	// Needs state
	TArray<FNeedEntry> CachedNeeds;

	// Draw toggles
	bool bDrawRadius = true;
	bool bDrawBuildings = true;
	bool bDrawNPCs = true;
	bool bDrawLinks = true;
	bool bDrawLabels = true;
	bool bDrawSOSlots = false;

	// Demand injection state
	int32 InjectDemandItemIdx = 0;
	int32 InjectDemandQuantity = 10;

	// Storage cap override
	bool bOverrideStorageCap = false;
	int32 OverriddenStorageCap = 500;

	// NPC state tree selection
	int32 SelectedStateFrameIdx = INDEX_NONE;
	uint16 SelectedStateIdx = MAX_uint16;

	// Draw actor
	TWeakObjectPtr<AActor> DrawActor;
	TWeakObjectPtr<UArcEconomyDebuggerDrawComponent> DrawComponent;
};
