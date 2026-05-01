// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcAreaTypes.h"
#include "ArcAreaSlotDefinition.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "ArcKnowledgeTypes.h"
class AActor;
class UArcAreaSubsystem;
class UArcAreaDebuggerDrawComponent;
class UArcKnowledgeSubsystem;

class FArcAreaDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	// --- Cached area entry for the list ---
	struct FAreaListEntry
	{
		FArcAreaHandle Handle;
		FString Label;
		FString DisplayName;
		FVector Location = FVector::ZeroVector;
		FGameplayTagContainer AreaTags;
		int32 TotalSlots = 0;
		int32 VacantSlots = 0;
		int32 AssignedSlots = 0;
		int32 ActiveSlots = 0;
		int32 DisabledSlots = 0;
		bool bHasSmartObject = false;
	};

	// --- Cached entity entry for NPC assignment ---
	struct FAssignableEntityEntry
	{
		FMassEntityHandle Entity;
		FString Label;
		FVector Location = FVector::ZeroVector;
		FGameplayTagContainer EligibleRoles;
		bool bCurrentlyAssigned = false;
		FArcAreaHandle CurrentAreaHandle;
		int32 CurrentSlotIndex = INDEX_NONE;
	};

	// --- Cached knowledge entry for the knowledge section ---
	struct FKnowledgeEntryCache
	{
		FArcKnowledgeHandle Handle;
		FGameplayTagContainer Tags;
		FVector Location = FVector::ZeroVector;
		float Relevance = 0.0f;
		double Timestamp = 0.0;
		float Lifetime = 0.0f;
		bool bIsAdvertisement = false;
		bool bClaimed = false;
		FMassEntityHandle ClaimedBy;
		FString PayloadTypeName;
	};

	// --- Cached claimable entity for knowledge claim popup ---
	struct FClaimableEntityEntry
	{
		FMassEntityHandle Entity;
		FString Label;
		FVector Location = FVector::ZeroVector;
		FGameplayTag Role;
	};

	// --- World helpers ---
	static UWorld* GetDebugWorld();
	UArcAreaSubsystem* GetAreaSubsystem() const;
	UArcKnowledgeSubsystem* GetKnowledgeSubsystem() const;

	// --- Refresh ---
	void RefreshAreaList();
	void RefreshAssignableEntities();

	// --- UI panels ---
	void DrawToolbar();
	void DrawAreaListPanel();
	void DrawAreaDetailPanel();
	void DrawSlotTable(const FArcAreaHandle& AreaHandle);
	void DrawSlotDefinitionDetails(const struct FArcAreaSlotDefinition& SlotDef, int32 SlotIndex);
	void DrawSlotRuntimeDetails(const struct FArcAreaSlotRuntime& SlotRuntime, int32 SlotIndex, const FArcAreaHandle& AreaHandle);
	void DrawSmartObjectSection(const struct FArcAreaData& AreaData);
	void DrawWorldVisualization();

	// --- Assignment popup ---
	void DrawAssignEntityPopup();

	// --- Knowledge section ---
	void RefreshKnowledgeEntries();
	void RefreshClaimableEntities();
	void DrawKnowledgeSection(const FMassEntityHandle& OwnerEntity);
	void DrawClaimEntityPopup();

	// --- State ---
	TArray<FAreaListEntry> CachedAreas;
	int32 SelectedAreaIndex = INDEX_NONE;
	int32 SelectedSlotIndex = INDEX_NONE;
	char AreaFilterBuf[256] = {};
	float LastRefreshTime = 0.f;

	// Draw toggles
	bool bAutoRefresh = true;
	bool bDrawAllAreas = true;
	bool bDrawSelectedDetail = true;
	bool bDrawLabels = true;

	// Assignment state
	bool bShowAssignPopup = false;
	int32 AssignTargetSlotIndex = INDEX_NONE;
	TArray<FAssignableEntityEntry> CachedAssignableEntities;
	char EntityFilterBuf[256] = {};

	// Knowledge section state
	TArray<FKnowledgeEntryCache> CachedKnowledgeEntries;
	int32 SelectedKnowledgeIndex = INDEX_NONE;
	FMassEntityHandle KnowledgeOwnerEntity;

	// Claim popup state
	bool bShowClaimPopup = false;
	FArcKnowledgeHandle ClaimTargetHandle;
	TArray<FClaimableEntityEntry> CachedClaimableEntities;
	char ClaimEntityFilterBuf[256] = {};

	// Draw actor
	TWeakObjectPtr<AActor> DrawActor;
	TWeakObjectPtr<UArcAreaDebuggerDrawComponent> DrawComponent;
	void EnsureDrawActor(UWorld* World);
	void DestroyDrawActor();
};
