// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"

class APlayerController;
class UWorld;

struct FArcTerrainInvokerViewState
{
	FVector LastGoodPosition = FVector::ZeroVector;
	bool bHasLastGoodPosition = false;
	bool bRejecting = false;
};

/** Caller-owned: one per gathering subsystem, never shared across worlds. */
struct FArcTerrainInvokerCache
{
	TMap<TWeakObjectPtr<const APlayerController>, FArcTerrainInvokerViewState> LastGoodByController;
	FArcTerrainInvokerViewState EditorView;
};

namespace ArcTerrain
{
	/**
	 * Quantized invoker positions for residency and collision. Game worlds add one
	 * GetPlayerViewPoint per player controller. A non-game Editor world adds the selected
	 * viewport only when no play session is active. Positions snap to 100 cm; identical
	 * snaps collapse.
	 *
	 * A non-finite or out-of-range view point emits that viewer's last accepted position instead of
	 * nothing, because an empty list means "no invoker" and routes the reconcile to a full-window
	 * teardown. OutNumSubstituted counts those; compare it against Num() when a phantom viewer is
	 * worse than none.
	 */
	ARCTERRAIN_API void GatherInvokers(const UWorld* World, FArcTerrainInvokerCache& Cache,
		TArray<FVector>& OutPositions, int32& OutNumSubstituted);

	/**
	 * The single point a diagnostic probe should follow: the local controller's camera, falling back
	 * to its pawn, falling back to the editor viewport outside PIE. Unlike GatherInvokers this never
	 * includes a remote pawn and is not quantized — it's a point probe, not a residency anchor.
	 * Returns false (leaving OutPosition untouched) when no such view exists, e.g. a dedicated server,
	 * or when the view it found is not a finite, in-range position.
	 */
	ARCTERRAIN_API bool GetViewPosition(const UWorld* World, FVector& OutPosition);
}
