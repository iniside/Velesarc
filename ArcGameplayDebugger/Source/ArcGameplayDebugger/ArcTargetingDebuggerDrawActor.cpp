// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTargetingDebuggerDrawActor.h"

#if UE_DEBUG_RENDERING_ENABLED
#include "DebugDraw.h"
#include "ArcTargetingPreset.h"
#include "ArcTargetingContext.h"
#include "Nodes/ArcTargetingNode.h"
#endif

AArcTargetingDebuggerDrawActor::AArcTargetingDebuggerDrawActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	bReplicates = false;
	SetReplicates(false);
}

void AArcTargetingDebuggerDrawActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

#if UE_DEBUG_RENDERING_ENABLED
	for (const FArcTargetingDebugTrace& Trace : Traces)
	{
		const UArcTargetingPreset* Preset = Trace.Preset.Get();
		if (Preset == nullptr)
		{
			continue;
		}

		FArcTargetingContext ReplayCtx;
		ReplayCtx.SourceActor    = const_cast<AActor*>(Trace.SourceActor.Get());
		ReplayCtx.SourceLocation = Trace.InitialCursor;

		for (const FArcTargetingDebugNodeEntry& E : Trace.Nodes)
		{
			if (!Preset->Nodes.IsValidIndex(E.NodeIndex)) { continue; }
			const FArcTargetingNode* Node = Preset->Nodes[E.NodeIndex].GetPtr<FArcTargetingNode>();
			if (Node == nullptr || !Node->bDrawDebug) { continue; }
			ReplayCtx.SourceLocation  = E.SourceLocationAfter;
			ReplayCtx.SourceDirection = E.SourceDirectionAfter;
			Node->DrawDebug(ReplayCtx, MakeArrayView(E.CandidatesBefore), MakeArrayView(E.CandidatesAfter));
		}
	}

	const FSceneInterface* Scene = UE::DebugDraw::GetSceneKey(GetWorld());
	for (const FArcTargetingDebugPersistentHit& Hit : PersistentHits)
	{
		UE::DebugDraw::Sphere(Scene, Hit.Location, 18.f, 12, Hit.Color);
	}
#endif // UE_DEBUG_RENDERING_ENABLED
}

#if UE_DEBUG_RENDERING_ENABLED
void AArcTargetingDebuggerDrawActor::SetTraces(TArray<FArcTargetingDebugTrace> InTraces)
{
	Traces = MoveTemp(InTraces);
}

void AArcTargetingDebuggerDrawActor::SetPersistentHits(TArray<FArcTargetingDebugPersistentHit> InHits)
{
	PersistentHits = MoveTemp(InHits);
}

void AArcTargetingDebuggerDrawActor::Clear()
{
	Traces.Reset();
	PersistentHits.Reset();
}
#endif // UE_DEBUG_RENDERING_ENABLED
