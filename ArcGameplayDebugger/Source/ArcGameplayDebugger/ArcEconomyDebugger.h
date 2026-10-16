// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "SmartObjectRuntime.h"
#include "Sim/ArcBodyAssignmentFragments.h"
#include "Sim/ArcBodyFragments.h"
#include "Sim/ArcBuildingFragments.h"
#include "Sim/ArcTransportJobFragment.h"
#include "UObject/PrimaryAssetId.h"

class UArcEconomyDebuggerDrawComponent;
class UArcSettlementLedgerSubsystem;
struct FMassEntityManager;

enum class EArcChainNodeKind : uint8 { Production, Gathering, ItemSource, Storage, ItemConsumer, GatherArea };
enum class EArcNPCEmploymentKind : uint8 { None, Work, Gather, Carrier };

struct FArcChainNodeKey
{
	FMassEntityHandle Entity;
	EArcChainNodeKind Kind = EArcChainNodeKind::Production;

	bool IsValid() const { return Entity.IsValid(); }
	void Reset() { Entity.Reset(); Kind = EArcChainNodeKind::Production; }
	bool operator==(const FArcChainNodeKey& Other) const { return Entity == Other.Entity && Kind == Other.Kind; }
	friend uint32 GetTypeHash(const FArcChainNodeKey& Key)
	{
		return HashCombine(GetTypeHash(Key.Entity), GetTypeHash(static_cast<uint8>(Key.Kind)));
	}
};

struct FArcChainViewNode
{
	FArcChainNodeKey Key;
	FString Name;
	TArray<FPrimaryAssetId> Inputs;
	TArray<FPrimaryAssetId> Outputs;
};

struct FArcChainViewEdge
{
	int32 FromIdx = INDEX_NONE;
	int32 ToIdx = INDEX_NONE;
	FPrimaryAssetId Item;
	bool bInFlight = false;
};

namespace ArcEconomyDebuggerShared
{
	UWorld* GetDebugWorld();
	FMassEntityManager* GetEntityManager();
	UArcSettlementLedgerSubsystem* GetLedger();

	const char* ProductionStateName(EArcProductionState State);
	const char* GatheringStateName(EArcGatheringState State);
	const char* BodyStateName(EArcBodyState State);

	FString EntityLabel(const FMassEntityManager& Manager, FMassEntityHandle Entity);
}

class FArcEconomyDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	struct FStoreLine
	{
		FPrimaryAssetId Item;
		int32 Count = 0;
	};

	struct FSlotEntry
	{
		FSmartObjectSlotHandle Handle;
		FMassEntityHandle Occupant;
		FString OccupantLabel;
		FString Role;
		ESmartObjectSlotState State = ESmartObjectSlotState::Invalid;
		bool bHasMassBehavior = false;
		bool bIsWorkerSlot = false;
		bool bHasSmartObjectSlot = false;

		int32 ProdSlotIndex = INDEX_NONE;
		FString RecipeName;
		float Progress = 0.0f;
		float CraftTime = 0.0f;
		float HaltDuration = 0.0f;
		EArcProductionState ProdState = EArcProductionState::Unstaffed;
		bool bHasProductionSlot = false;
	};

	struct FBuildingEntry
	{
		FMassEntityHandle Entity;
		FString Name;
		FVector Location = FVector::ZeroVector;
		bool bProduction = false;
		bool bGathering = false;
		bool bItemSource = false;
		bool bStorage = false;
		bool bItemConsumer = false;
		bool bService = false;
		bool bStaffing = false;
		bool bGatherArea = false;

		bool bHasStaff = false;
		int32 StaffCount = 0;
		int32 StaffRequired = 0;
		bool bHasEmployment = false;
		int32 EmploymentCount = 0;

		bool bHasProductionState = false;
		EArcProductionState ProductionState = EArcProductionState::Unstaffed;
		int32 ProducingSlotCount = 0;
		int32 AssignedSlotCount = 0;
		int32 OccupiedWorkerSlotCount = 0;
		bool bHasGatheringState = false;
		EArcGatheringState GatheringState = EArcGatheringState::Unstaffed;

		FPrimaryAssetId ItemSourceItem;
		FPrimaryAssetId AreaItem;
		FGameplayTag AreaTag;

		TArray<FStoreLine> Inputs;
		TArray<FStoreLine> Outputs;
		TArray<FSlotEntry> Slots;
	};

	struct FNPCEntry
	{
		FMassEntityHandle Entity;
		FString Label;
		EArcNPCEmploymentKind Kind = EArcNPCEmploymentKind::None;
		FString StateText;
		FVector Location = FVector::ZeroVector;

		FMassEntityHandle Workplace;
		FMassEntityHandle GatherArea;
		FMassEntityHandle Source;
		FMassEntityHandle Sink;
		bool bHasUtilityIntent = false;
		FGameplayTag UtilityGoal;
		uint32 UtilityCommandSerial = 0;

		FPrimaryAssetId GatherItem;
		int32 GatherQuantity = 0;
		uint32 GatherCommandSerial = 0;
		EArcGatherAssignmentPhase GatherPhase = EArcGatherAssignmentPhase::Gather;

		FPrimaryAssetId Item;
		int32 Quantity = 0;
		bool bHasTransportJob = false;
		uint32 TransportGeneration = 0;
		uint32 TransportCommandSerial = 0;
		EArcTransportJobPhase TransportPhase = EArcTransportJobPhase::Pickup;

		TArray<FStoreLine> Cargo;
	};

	struct FSettlementEntry
	{
		FMassEntityHandle Entity;
		FString Name;
		FVector Location = FVector::ZeroVector;
		FVector Extent = FVector::ZeroVector;
		FTransform Transform;
	};

	void RefreshAll();
	int32 FindBuildingIndex(FMassEntityHandle Building) const;
	void PickNPCInViewport(const FMassEntityManager& Manager);
	void UpdateDrawComponent();
	void EnsureDrawActor(UWorld* World);
	void DestroyDrawActor();
	void DrawLedgerTab();
	void DrawSpawnSection();
	void RefreshEntityConfigs();

	void RefreshBuildingList();
	void RefreshSettlementList();
	void DrawBuildingsTab();
	void DrawBuildingDetail();

	void RefreshNPCList();
	void DrawWorkforceTab();

	void DrawNPCDetailPanel();

	void RefreshProductionChain();
	void DrawProductionChainTab();

	TArray<FBuildingEntry> CachedBuildings;
	int32 SelectedBuildingIdx = INDEX_NONE;
	float BuildingListWidthFraction = 0.38f;

	TArray<FSettlementEntry> CachedSettlements;
	int32 SelectedSettlementIdx = INDEX_NONE;
	int32 SpawnUnassignedCount = 1;

	TArray<FNPCEntry> CachedNPCs;
	int32 SelectedNPCIdx = INDEX_NONE;
	float WorkforceListWidthFraction = 0.62f;

	TArray<FArcChainViewNode> ChainNodes;
	TArray<FArcChainViewEdge> ChainEdges;
	TArray<FString> ChainDiagnostics;
	TMap<FArcChainNodeKey, FVector2D> ManualBoxPositions;
	FArcChainNodeKey DraggedBoxKey;
	FArcChainNodeKey SelectedGraphNode;
	FVector2D DragStartPos = FVector2D::ZeroVector;
	FVector2D DragPressMousePos = FVector2D::ZeroVector;
	bool bDragExceededThreshold = false;
	FVector2D GraphViewOffset = FVector2D::ZeroVector;
	bool bPanningCanvas = false;
	FArcChainNodeKey HoveredNodeKey;

	TArray<FAssetData> CachedEntityConfigs;
	TArray<FString> CachedEntityConfigNames;
	int32 SelectedEntityConfigIdx = INDEX_NONE;
	int32 SpawnBodyKindIdx = 0;
	int32 SpawnBuildingIdx = INDEX_NONE;
	FString LastSpawnMessage;

	bool bDrawBuildings = true;
	bool bDrawNPCs = true;
	bool bDrawLinks = true;
	bool bDrawLabels = true;

	float LastRefreshTime = 0.0f;

	TWeakObjectPtr<AActor> DrawActor;
	TWeakObjectPtr<UArcEconomyDebuggerDrawComponent> DrawComponent;
};
