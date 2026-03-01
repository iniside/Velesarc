// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcKnowledgeTypes.h"
#include "ArcKnowledgeEntry.h"
#include "GameplayTagContainer.h"

class UArcKnowledgeSubsystem;

class FArcKnowledgeDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	// --- Cached entry for the list ---
	struct FKnowledgeListEntry
	{
		FArcKnowledgeHandle Handle;
		FString Label;
		FVector Location = FVector::ZeroVector;
		FGameplayTagContainer Tags;
		float Relevance = 0.f;
		bool bClaimed = false;
		bool bHasPayload = false;
		float BoundingRadius = 0.f; // Only set if definition provides it
	};

	// --- World helpers ---
	static UWorld* GetDebugWorld();
	UArcKnowledgeSubsystem* GetKnowledgeSubsystem() const;

	// --- Refresh ---
	void RefreshEntryList();

	// --- UI panels ---
	void DrawToolbar();
	void DrawEntryListPanel();
	void DrawEntryDetailPanel();
	void DrawWorldVisualization();

	// --- Entry detail sub-sections ---
	void DrawTagsSection(const FArcKnowledgeEntry& Entry, FArcKnowledgeHandle Handle);
	void DrawPayloadSection(const FArcKnowledgeEntry& Entry);
	void DrawAdvertisementSection(const FArcKnowledgeEntry& Entry, FArcKnowledgeHandle Handle);
	void DrawMetadataSection(const FArcKnowledgeEntry& Entry);

	// --- Payload property rendering ---
	void DrawPayloadProperties(const UScriptStruct* PayloadType, const void* PayloadData);

	// --- Entry mutation ---
	void DrawAddEntryPopup();
	void DrawAddTagPopup(FArcKnowledgeHandle Handle);
	void DrawRemoveTagPopup(FArcKnowledgeHandle Handle, const FGameplayTagContainer& CurrentTags);

	// --- State ---
	TArray<FKnowledgeListEntry> CachedEntries;
	int32 SelectedEntryIndex = INDEX_NONE;
	char EntryFilterBuf[256] = {};
	float LastRefreshTime = 0.f;

	// Draw toggles
	bool bDrawAllEntries = true;
	bool bDrawSelectedRadius = true;
	bool bDrawLabels = true;
	bool bAutoRefresh = true;

	// Add entry state
	bool bShowAddEntryPopup = false;
	char AddEntryTagsBuf[512] = {};
	float AddEntryLocation[3] = {0.f, 0.f, 0.f};
	float AddEntryRelevance = 1.0f;

	// Tag mutation state
	bool bShowAddTagPopup = false;
	bool bShowRemoveTagPopup = false;
	char AddTagBuf[256] = {};
};
