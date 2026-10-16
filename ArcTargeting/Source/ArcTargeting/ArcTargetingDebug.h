#pragma once

#include "CoreMinimal.h"
#include "ArcTargetingPipelineTypes.h"
#include "VisualLogger/VisualLoggerDefines.h"
#include "ArcTargetingDebug.generated.h"

class AActor;
class UArcTargetingPreset;
class FSceneInterface;
struct FArcTargetingContext;

/**
 * Per-request debug mode. The effective mode is `max(GlobalCVarMode, RequestMode)`
 * so the CVar acts as a force-on floor.
 *
 * Off     — DrawDebug is never invoked; no capture; zero overhead.
 * Live    — DrawDebug invoked once during ExecuteRequestAtomic. Primitives clear
 *           on the next OnEndFrame, so visible for one frame only.
 * Persist — Capture per-node observations into a snapshot, then re-emit every Tick
 *           until the request is released. Lets PIE users walk around results.
 */
UENUM()
enum class EArcTargetingDebugMode : uint8
{
	Off,
	Live,
	Persist
};

#if UE_DEBUG_RENDERING_ENABLED

/**
 * Scene key for the DebugDraw submissions a node's DrawDebug makes.
 * Resolved from the context's source actor, then its source object; null when neither
 * resolves, which submits scene-agnostic primitives that render in every viewport.
 */
ARCTARGETING_API const FSceneInterface* GetTargetingDebugScene(const FArcTargetingContext& Ctx);

/**
 * One node execution captured for Persist-mode replay.
 * CandidatesBefore/After snapshots are sufficient for all node kinds:
 * trace/overlap nodes added entries past Before.Num(); filter nodes removed
 * entries; score nodes mutated Score in place. SourceLocation/Direction are
 * captured post-execute so Origin nodes that rewrite the cursor are replayed
 * correctly.
 */
struct FArcTargetingDebugNodeEntry
{
	int32 NodeIndex = INDEX_NONE;
	TArray<FArcTargetCandidate> CandidatesBefore;
	TArray<FArcTargetCandidate> CandidatesAfter;
	FVector SourceLocationAfter = FVector::ZeroVector;
	FVector SourceDirectionAfter = FVector::ForwardVector;
};

/**
 * Per-node observations captured in Persist mode and replayed every Tick.
 * Node resolved at replay time via Preset->Nodes[NodeIndex].
 */
struct FArcTargetingDebugSnapshot
{
	TArray<FArcTargetingDebugNodeEntry> Nodes;

	void Reset() { Nodes.Reset(); }
};

/**
 * Persistent-hit record for a single candidate position. Used by the draw
 * actor in ArcGameplayDebugger to re-emit per-candidate markers each frame.
 */
struct FArcTargetingDebugPersistentHit
{
	FVector Location = FVector::ZeroVector;
	float Score = 0.f;
	FColor Color = FColor::White;
	FString Label;
};

/**
 * One completed targeting pass captured for history replay.
 * Nodes are resolved at replay time via Preset.Get()->Nodes[NodeIndex].
 * Safe to hold in a ring buffer across frames.
 */
struct FArcTargetingDebugTrace
{
	/** Preset that produced the trace. Held weakly so unloading is safe. */
	TWeakObjectPtr<const UArcTargetingPreset> Preset;

	/** Actor that issued the request. */
	TWeakObjectPtr<const AActor> SourceActor;

	/** FPlatformTime::Seconds() at the moment ExecuteRequestAtomic returned. */
	double CompletedRealTimeSeconds = 0.0;

	/** Context.SourceLocation at pipeline start (before any node runs). */
	FVector InitialCursor = FVector::ZeroVector;

	TArray<FArcTargetingDebugNodeEntry> Nodes;

	/** Candidate array after the final node ran. */
	TArray<FArcTargetCandidate> FinalCandidates;
};

#else

struct FArcTargetingDebugSnapshot
{
};

/** Stub so delegate declarations compile in non-debug configurations. */
struct FArcTargetingDebugTrace
{
};

#endif // UE_DEBUG_RENDERING_ENABLED
