// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcAreaTypes.h"
#include "ArcAreaSlotDefinition.h"
#include "GameplayTagContainer.h"
#include "MassEntityHandle.h"

class UArcAreaSubsystem;

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

	// --- World helpers ---
	static UWorld* GetDebugWorld();
	UArcAreaSubsystem* GetAreaSubsystem() const;

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

	// --- State ---
	TArray<FAreaListEntry> CachedAreas;
	int32 SelectedAreaIndex = INDEX_NONE;
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
};
