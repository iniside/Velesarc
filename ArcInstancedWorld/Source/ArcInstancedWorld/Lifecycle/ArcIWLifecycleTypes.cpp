// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWLifecycleTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcIWLifecycleTypes)

const FArcIWLifecycleMeshOverride* FArcIWLifecycleVisConfigFragment::FindOverride(uint8 Phase) const
{
	for (const FArcIWLifecycleMeshOverride& Override : MeshOverrides)
	{
		if (Override.Phase == Phase)
		{
			return &Override;
		}
	}
	return nullptr;
}

namespace UE::ArcIW::Lifecycle
{

FArcIWMeshEntry ResolvePhaseOverride(
	const FArcIWMeshEntry& BaseMesh,
	const FArcIWLifecycleMeshOverride& Override)
{
	FArcIWMeshEntry Result = BaseMesh;

	if (Override.MeshOverride)
	{
		Result.Mesh = Override.MeshOverride;
	}

	if (Override.SkinnedAssetOverride)
	{
		Result.SkinnedAsset = Override.SkinnedAssetOverride;
	}

	if (Override.MaterialOverrides.Num() > 0)
	{
		Result.Materials = Override.MaterialOverrides;
	}

	return Result;
}

} // namespace UE::ArcIW::Lifecycle
