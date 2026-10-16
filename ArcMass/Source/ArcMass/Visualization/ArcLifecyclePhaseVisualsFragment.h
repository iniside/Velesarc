// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "ArcMass/Visualization/ArcVisLifecycle.h"
#include "ArcLifecyclePhaseVisualsFragment.generated.h"

class UStaticMesh;
class UMaterialInterface;
struct FArcMassStaticMeshConfigFragment;
struct FArcVisConfigFragment;

/**
 * Const-shared fragment carrying per-phase visual overrides.
 * Authored via UArcVisLifecycleTrait; consumed by UArcVisLifecycleVisSwitchProcessor
 * and UArcVisMeshActivateProcessor. Mark the requirement Optional so non-visual
 * lifecycle entities do not need to author vis data.
 */
USTRUCT(BlueprintType)
struct ARCMASS_API FArcLifecyclePhaseVisualsFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Lifecycle|Visuals")
	TArray<FArcVisLifecyclePhaseVisuals> PhaseVisuals;

	/** Returns the phase-visuals entry for the given phase index, or nullptr if out of range. */
	const FArcVisLifecyclePhaseVisuals* GetPhaseVisuals(uint8 Phase) const;

	/**
	 * Resolve the static mesh to use for the given phase.
	 * Falls back to BaseMeshConfigFrag.Mesh if the phase has no override.
	 */
	UStaticMesh* ResolveMesh(uint8 Phase, const FArcMassStaticMeshConfigFragment& BaseMeshConfigFrag) const;

	/**
	 * Resolve the material array to use for the given phase.
	 * Falls back to BaseConfig.MaterialOverrides if the phase has no override.
	 */
	const TArray<TObjectPtr<UMaterialInterface>>& ResolveMaterials(uint8 Phase, const FArcVisConfigFragment& BaseConfig) const;
};

template<>
struct TMassFragmentTraits<FArcLifecyclePhaseVisualsFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};
