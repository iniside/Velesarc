// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "AssetActionUtility.h"

#include "ArcSkeletalMeshRandomWeightsUtility.generated.h"

class USkeletalMesh;

/**
 * Right-click action on a USkeletalMesh asset that overwrites LOD0 skin weights so
 * every vertex is fully bound (weight = 1.0) to a single randomly-chosen bone.
 * Intended for trivial test meshes (e.g. a plane skinned to a placeholder skeleton).
 */
UCLASS()
class UArcSkeletalMeshRandomWeightsUtility : public UAssetActionUtility
{
	GENERATED_BODY()

public:
	UArcSkeletalMeshRandomWeightsUtility();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Arc|SkeletalMesh")
	void AssignRandomBoneWeights(USkeletalMesh* SkeletalMesh, int32 LODIndex = 0, int32 RandomSeed = 0);
};
