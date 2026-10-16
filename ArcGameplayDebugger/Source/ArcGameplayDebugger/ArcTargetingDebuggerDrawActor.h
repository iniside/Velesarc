// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArcTargetingDebug.h"
#include "ArcTargetingDebuggerDrawActor.generated.h"

/**
 * Transient actor spawned by FArcTargetingDebugger when its window opens.
 * Owns a copy of selected traces and re-emits DrawDebug calls via op dispatch
 * on every Tick. PersistentHits are drawn as spheres independently.
 *
 * Lifetime is bound to the debugger window — destroying the actor stops
 * all visuals next frame, so closing the panel cleans up automatically.
 */
UCLASS(NotPlaceable, Transient, NotBlueprintable)
class AArcTargetingDebuggerDrawActor : public AActor
{
	GENERATED_BODY()

public:
	AArcTargetingDebuggerDrawActor();

	virtual void Tick(float DeltaSeconds) override;

#if UE_DEBUG_RENDERING_ENABLED
	void SetTraces(TArray<FArcTargetingDebugTrace> InTraces);
	void SetPersistentHits(TArray<FArcTargetingDebugPersistentHit> InHits);
	void Clear();

private:
	TArray<FArcTargetingDebugTrace> Traces;
	TArray<FArcTargetingDebugPersistentHit> PersistentHits;
#endif
};
