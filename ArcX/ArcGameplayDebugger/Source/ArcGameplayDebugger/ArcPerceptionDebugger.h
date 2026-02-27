// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityHandle.h"
#include "MassEntityTypes.h"

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

	// --- World drawing helpers ---
	void DrawSenseRange(const FVector& Origin, const FVector& Forward, const struct FArcPerceptionSenseConfigFragment* Config,
		const FColor& Color) const;
	void DrawPerceivedEntityMarker(const struct FArcPerceivedEntity& PE, const FColor& Color, bool bHighlighted,
		struct FMassEntityManager* Manager) const;

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
};
