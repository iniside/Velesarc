// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/EntityHandle.h"
#include "MassEntityTypes.h"
#include "GameplayTagContainer.h"

class FArcAIDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	void DrawEntityListPanel();
	void DrawEntityDetailPanel();
	void DrawSelectedStatePanel();
	void DrawPlanInWorld();

	void DrawSendAsyncMessage();
	void DrawSendStateTreeEvent();

	void RefreshEntityList();

	struct FEntityEntry
	{
		FMassEntityHandle Entity;
		FString Label;
		FVector Location = FVector::ZeroVector;
	};

	TArray<FEntityEntry> CachedEntities;
	int32 SelectedEntityIndex = INDEX_NONE;
	char EntityFilterBuf[256] = {};
	float LastRefreshTime = 0.f;

	// Selected state in the tree
	int32 SelectedStateFrameIdx = INDEX_NONE;
	uint16 SelectedStateIdx = MAX_uint16;
	enum class ENestedTreeType : uint8 { None, SmartObject, Advertisement };
	ENestedTreeType SelectedNestedType = ENestedTreeType::None;
	int32 NestedTaskStorageIdx = INDEX_NONE;

	// Send Async Message state
	char AsyncMessageTagBuf[256] = {};

	// Send StateTree Event state
	char StateTreeEventTagBuf[256] = {};
};
