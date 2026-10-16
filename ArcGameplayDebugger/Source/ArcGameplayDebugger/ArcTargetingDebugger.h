// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Persistent/ArcTargetingPersistentEntry.h"

class AArcTargetingDebuggerDrawActor;

/**
 * ImGui debugger panel for ArcTargeting. Follows the debugger panel
 * pattern (Initialize / Uninitialize / Draw + bShow) consumed by
 * UArcGameplayDebuggerSubsystem::ToggleDebuggerMenuItem and DrawIfVisible.
 *
 * The full panel body is compiled only when UE_DEBUG_RENDERING_ENABLED is set.
 * Outside that gate Initialize/Uninitialize/Draw are inline no-ops so the
 * subsystem wiring compiles in all configurations.
 */
class FArcTargetingDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

#if UE_DEBUG_RENDERING_ENABLED
private:
	/** Indices into UArcTargetingSubsystem::GetRecentTraces() that are highlighted. */
	TSet<int32> SelectedHistoryIndices;

	TSet<FArcTargetingPersistentKey> SelectedGlobalKeys;

	/** When true, the most-recent trace is auto-selected each Draw. */
	bool bAutoSelectLatest = false;

	/** Per-kind visibility mask in the detail panel. Aligns with EArcTargetingNodeKind. */
	bool bShowOrigin  = true;
	bool bShowTrace   = true;
	bool bShowOverlap = true;
	bool bShowFilter  = true;
	bool bShowScore   = true;
	bool bShowSort    = true;
	bool bShowMisc    = true;

	/** Tracks whether we have bumped the subsystem's history refcount. */
	bool bRefCountBumped = false;

	/** Transient actor that re-emits debug shapes each tick while the window is open. */
	TWeakObjectPtr<AArcTargetingDebuggerDrawActor> DrawActor;

	/** World the debugger was initialised against (held weakly). */
	TWeakObjectPtr<UWorld> CachedWorld;
#endif // UE_DEBUG_RENDERING_ENABLED
};
