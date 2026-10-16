// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcLifecyclePhaseVisualsFragment.h"
#include "ArcMassVisualizationConfigFragments.h"
#include "ArcMassEntityVisualization.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcLifecyclePhaseVisualsFragment)

namespace ArcLifecyclePhaseVisualsFragmentInternal
{
	static const FArcVisLifecyclePhaseVisuals GEmptyPhaseVisuals;
}

const FArcVisLifecyclePhaseVisuals* FArcLifecyclePhaseVisualsFragment::GetPhaseVisuals(uint8 Phase) const
{
	return PhaseVisuals.IsValidIndex(Phase) ? &PhaseVisuals[Phase] : nullptr;
}

UStaticMesh* FArcLifecyclePhaseVisualsFragment::ResolveMesh(uint8 Phase, const FArcMassStaticMeshConfigFragment& BaseMeshConfigFrag) const
{
	const FArcVisLifecyclePhaseVisuals* Visuals = GetPhaseVisuals(Phase);
	if (Visuals && Visuals->HasMeshOverride())
	{
		return Visuals->StaticMesh.Get();
	}
	return const_cast<UStaticMesh*>(BaseMeshConfigFrag.Mesh.Get());
}

const TArray<TObjectPtr<UMaterialInterface>>& FArcLifecyclePhaseVisualsFragment::ResolveMaterials(
	uint8 Phase, const FArcVisConfigFragment& BaseConfig) const
{
	const FArcVisLifecyclePhaseVisuals* Visuals = GetPhaseVisuals(Phase);
	if (Visuals && Visuals->MaterialOverrides.Num() > 0)
	{
		return Visuals->MaterialOverrides;
	}
	return BaseConfig.MaterialOverrides;
}
