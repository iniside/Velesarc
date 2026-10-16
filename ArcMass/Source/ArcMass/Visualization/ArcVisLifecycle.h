// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "ArcVisLifecycle.generated.h"

class UStaticMesh;
class UMaterialInterface;

// ---------------------------------------------------------------------------
// Per-Phase Visual Descriptor
// (kept here because UArcVisLifecycleTrait and FArcLifecyclePhaseVisualsFragment
//  both reference it; the fragment and subsystem are in ArcMassLifecycle.h)
// ---------------------------------------------------------------------------

/** Describes visual overrides for a single lifecycle phase.
  * Any nullptr/empty field inherits from the base FArcVisConfigFragment. */
USTRUCT(BlueprintType)
struct ARCMASS_API FArcVisLifecyclePhaseVisuals
{
	GENERATED_BODY()

	/** Static mesh for this phase's ISM representation. nullptr = inherit from base config. */
	UPROPERTY(EditAnywhere, Category = "Visualization")
	TObjectPtr<UStaticMesh> StaticMesh = nullptr;

	/** Material overrides for this phase. Empty = inherit from base config. */
	UPROPERTY(EditAnywhere, Category = "Visualization")
	TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides;

	bool HasMeshOverride() const { return StaticMesh != nullptr; }
};
