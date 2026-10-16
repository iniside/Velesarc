// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "Delegates/IDelegateInstance.h"
#include "HAL/CriticalSection.h"
#include "imgui.h"
#include "Mass/EntityHandle.h"
#include "Mass/EntityMacros.h"
#include "UObject/ObjectKey.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UMassCompositeProcessor;
class UMassProcessor;
class UMassSignalSubsystem;

#if WITH_MASSENTITY_DEBUG

struct FMassEntityManager;
struct FMassEntityQuery;

namespace UE::Mass::Debug
{
	struct FQueryRequirementsView;
}

struct FArcMassProcessorNode
{
	TWeakObjectPtr<UMassProcessor> Processor;
	FName Name;
	FString DisplayName;
	FName GroupName;
	int32 PhaseIndex = INDEX_NONE;
	bool bScheduled = false;
	bool bSignalProcessor = false;
	TArray<FName> RegisteredSignals;
	int32 Layer = 0;
	ImVec2 GraphPos = ImVec2(0.f, 0.f);
};

struct FArcMassProcessorEdge
{
	int32 FromIndex = INDEX_NONE;
	int32 ToIndex = INDEX_NONE;
};

#endif // WITH_MASSENTITY_DEBUG

class FArcMassProcessorDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

#if WITH_MASSENTITY_DEBUG
private:
	void RefreshIfNeeded();
	void RebuildGraph();
	void CollectProcessors(const UMassCompositeProcessor& Composite, int32 PhaseIndex);
	void CollectEdges(const UMassCompositeProcessor& Composite);
	void BuildLayout();
	void BindSignalHooks();
	void UnbindSignalHooks();
	bool IsGraphStale() const;
	void DrawToolbar();
	void HandleCanvasInput();
	void DrawGraph();
	ImVec2 GraphToScreen(const ImVec2& GraphPos) const;
	FVector2D ScreenToGraph(const ImVec2& ScreenPos) const;
	bool IsNodeVisible(int32 NodeIndex) const;
	int32 FindNodeAtScreenPos(const ImVec2& ScreenPos) const;
	void DrawDetailPanel();
	void DrawQueryRequirements(const UE::Mass::Debug::FQueryRequirementsView& Requirements);
	void DrawSubsystemRequirements(const UE::Mass::Debug::FQueryRequirementsView& Requirements, const FMassEntityManager& EntityManager);
	void DrawMatchedEntities(TConstArrayView<FMassEntityQuery*> Queries, const FMassEntityManager& EntityManager);
	void RefreshMatchedEntities(TConstArrayView<FMassEntityQuery*> Queries, const FMassEntityManager& EntityManager);
	void DrawSignalSection(const FArcMassProcessorNode& Node, const UMassProcessor& Processor);

	TArray<FArcMassProcessorNode> Nodes;
	TArray<FArcMassProcessorEdge> Edges;
	TArray<FString> GraphDiagnostics;

	TMap<const UMassProcessor*, int32> NodeByProcessor;
	TArray<const UMassCompositeProcessor*> VisitedComposites;

	TMap<TObjectKey<UMassProcessor>, ImVec2> ManualPositions;
	TMap<TObjectKey<UMassProcessor>, double> LastSignalTimes;
	FCriticalSection SignalTimesCS;

	TArray<TPair<FName, FDelegateHandle>> SignalHandles;
	TWeakObjectPtr<UMassSignalSubsystem> BoundSignalSubsystem;

	TArray<TWeakObjectPtr<const UMassCompositeProcessor>> CachedPhaseRoots;
	int32 CachedProcessorCount = 0;
	bool bRebuildPending = true;

	FVector2D ViewOffset = FVector2D::ZeroVector;
	float Zoom = 1.f;
	float MinZoom = 0.1f;
	float MaxZoom = 4.f;
	bool bIsPanning = false;
	int32 PanMouseButton = ImGuiMouseButton_Middle;
	ImVec2 PanStartMouse = ImVec2(0.f, 0.f);
	FVector2D PanStartOffset = FVector2D::ZeroVector;
	ImVec2 CanvasPos = ImVec2(0.f, 0.f);
	ImVec2 CanvasSize = ImVec2(0.f, 0.f);
	ImVec2 CanvasCenter = ImVec2(0.f, 0.f);
	TObjectKey<UMassProcessor> SelectedProcessorKey;
	int32 SelectedNodeIndex = INDEX_NONE;
	int32 HoveredNodeIndex = INDEX_NONE;
	int32 DraggedNodeIndex = INDEX_NONE;
	bool bShowUnscheduled = true;
	char NameFilterBuf[128] = {};

	TArray<FMassEntityHandle> CachedMatchedEntities;
	TObjectKey<UMassProcessor> CachedEntitiesProcessorKey;
	bool bMatchedEntitiesValid = false;
#endif // WITH_MASSENTITY_DEBUG
};
