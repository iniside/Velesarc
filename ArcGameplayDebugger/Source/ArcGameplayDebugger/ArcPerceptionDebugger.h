// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/EntityHandle.h"
#include "MassEntityTypes.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AActor;
class UArcPerceptionDebuggerDrawComponent;

class FArcPerceptionDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	// --- Entity list (left panel) ---
	struct FPerceiverEntry
	{
		FMassEntityHandle Entity;
		FString Label;
		FVector Location = FVector::ZeroVector;
		bool bHasSight = false;
		bool bHasHearing = false;
	};

	void RefreshPerceiverList();
	void DrawPerceiverListPanel();
	void DrawPerceiverDetailPanel();
	void DrawWorldVisualization();

	// --- Sense details ---
	void DrawSenseSection(const TCHAR* SenseName, struct FArcMassPerceptionResultFragmentBase* Result,
		const struct FArcPerceptionSenseConfigFragment* Config, bool& bDrawShapes, float CurrentTime);
	void DrawPerceivedEntityRow(const struct FArcPerceivedEntity& PE, int32 Index, const struct FArcPerceptionSenseConfigFragment* Config,
		float CurrentTime, struct FMassEntityManager* Manager);
	void DrawSenseConfig(const struct FArcPerceptionSenseConfigFragment* Config);

	void EnsureDrawActor(UWorld* World);
	void DestroyDrawActor();

	TArray<FPerceiverEntry> CachedPerceivers;
	int32 SelectedPerceiverIndex = INDEX_NONE;
	char PerceiverFilterBuf[256] = {};
	float LastRefreshTime = 0.f;

	// Draw toggles
	bool bDrawSightShapes = true;
	bool bDrawHearingShapes = true;
	bool bDrawPerceivedPositions = true;

	// Selected perceived entity (for highlighting)
	FMassEntityHandle HighlightedPerceivedEntity;

	// Draw component
	TWeakObjectPtr<AActor> DrawActor;
	TWeakObjectPtr<UArcPerceptionDebuggerDrawComponent> DrawComponent;
};
