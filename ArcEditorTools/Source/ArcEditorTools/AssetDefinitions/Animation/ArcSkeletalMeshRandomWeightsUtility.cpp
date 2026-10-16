// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/Animation/ArcSkeletalMeshRandomWeightsUtility.h"

#include "Engine/SkeletalMesh.h"
#include "BoneWeights.h"
#include "MeshDescription.h"
#include "ReferenceSkeleton.h"
#include "SkeletalMeshAttributes.h"
#include "ScopedTransaction.h"
#include "Logging/LogMacros.h"

#define LOCTEXT_NAMESPACE "ArcSkeletalMeshRandomWeightsUtility"

DEFINE_LOG_CATEGORY_STATIC(LogArcSkeletalMeshRandomWeights, Log, All);

namespace ArcSkeletalMeshRandomWeightsUtility_Private
{
	static bool AssignFullCoverageWeightsToLOD(USkeletalMesh* SkeletalMesh, int32 LODIndex)
	{
		FMeshDescription* MeshDescription = SkeletalMesh->GetMeshDescription(LODIndex);
		if (MeshDescription == nullptr)
		{
			UE_LOG(LogArcSkeletalMeshRandomWeights, Warning,
				TEXT("Skeletal mesh '%s' has no MeshDescription for LOD %d."),
				*SkeletalMesh->GetName(), LODIndex);
			return false;
		}

		const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
		const int32 BoneCount = RefSkeleton.GetNum();
		if (BoneCount <= 0)
		{
			UE_LOG(LogArcSkeletalMeshRandomWeights, Warning,
				TEXT("Skeletal mesh '%s' has no bones in its reference skeleton."),
				*SkeletalMesh->GetName());
			return false;
		}

		FSkeletalMeshAttributes MeshAttributes(*MeshDescription);
		FSkinWeightsVertexAttributesRef SkinWeights = MeshAttributes.GetVertexSkinWeights();
		if (!SkinWeights.IsValid())
		{
			UE_LOG(LogArcSkeletalMeshRandomWeights, Warning,
				TEXT("Skeletal mesh '%s' LOD %d has no skin weight attribute."),
				*SkeletalMesh->GetName(), LODIndex);
			return false;
		}

		TArray<FVertexID> VertexIDs;
		for (const FVertexID VertexID : MeshDescription->Vertices().GetElementIDs())
		{
			VertexIDs.Add(VertexID);
		}
		const int32 VertexCount = VertexIDs.Num();
		if (VertexCount <= 0)
		{
			UE_LOG(LogArcSkeletalMeshRandomWeights, Warning,
				TEXT("Skeletal mesh '%s' LOD %d has no vertices."),
				*SkeletalMesh->GetName(), LODIndex);
			return false;
		}

		const int32 MaxInfluences = UE::AnimationCore::MaxInlineBoneWeightCount;
		const int32 MaxAssignableBones = VertexCount * MaxInfluences;
		const bool bExceedsCapacity = BoneCount > MaxAssignableBones;
		if (bExceedsCapacity)
		{
			UE_LOG(LogArcSkeletalMeshRandomWeights, Warning,
				TEXT("Skeletal mesh '%s' LOD %d: %d bones exceed capacity %d (verts %d * max influences %d). Bones beyond capacity will be unbound."),
				*SkeletalMesh->GetName(), LODIndex, BoneCount, MaxAssignableBones, VertexCount, MaxInfluences);
		}

		TArray<TArray<FBoneIndexType, TInlineAllocator<8>>> BonesPerVertex;
		BonesPerVertex.SetNum(VertexCount);

		// Pass 1: round-robin every bone onto a vertex so every bone is referenced at least once.
		const int32 BonesToAssign = bExceedsCapacity ? MaxAssignableBones : BoneCount;
		for (int32 BoneIdx = 0; BoneIdx < BonesToAssign; ++BoneIdx)
		{
			const int32 VertexSlot = BoneIdx % VertexCount;
			BonesPerVertex[VertexSlot].Add(static_cast<FBoneIndexType>(BoneIdx));
		}

		// Pass 2: any vertex with no bones (VertexCount > BoneCount case) gets a round-robin fallback.
		for (int32 VertexIdx = 0; VertexIdx < VertexCount; ++VertexIdx)
		{
			if (BonesPerVertex[VertexIdx].Num() == 0)
			{
				BonesPerVertex[VertexIdx].Add(static_cast<FBoneIndexType>(VertexIdx % BoneCount));
			}
		}

		TArray<UE::AnimationCore::FBoneWeight, TInlineAllocator<8>> WeightsScratch;
		for (int32 VertexIdx = 0; VertexIdx < VertexCount; ++VertexIdx)
		{
			const TArray<FBoneIndexType, TInlineAllocator<8>>& AssignedBones = BonesPerVertex[VertexIdx];
			const float PerBoneWeight = 1.0f / static_cast<float>(AssignedBones.Num());

			WeightsScratch.Reset();
			for (const FBoneIndexType BoneIdx : AssignedBones)
			{
				WeightsScratch.Emplace(BoneIdx, PerBoneWeight);
			}

			SkinWeights.Set(VertexIDs[VertexIdx], TArrayView<const UE::AnimationCore::FBoneWeight>(WeightsScratch));
		}

		UE_LOG(LogArcSkeletalMeshRandomWeights, Log,
			TEXT("Bound %d vertices to %d bones on '%s' LOD %d (full-coverage round-robin)."),
			VertexCount, BonesToAssign, *SkeletalMesh->GetName(), LODIndex);

		USkeletalMesh::FCommitMeshDescriptionParams CommitParams;
		CommitParams.bForceUpdate = true;
		return SkeletalMesh->CommitMeshDescription(LODIndex, CommitParams);
	}
}

UArcSkeletalMeshRandomWeightsUtility::UArcSkeletalMeshRandomWeightsUtility()
{
	SupportedClasses.Add(USkeletalMesh::StaticClass());
}

void UArcSkeletalMeshRandomWeightsUtility::AssignRandomBoneWeights(USkeletalMesh* SkeletalMesh, int32 LODIndex, int32 RandomSeed)
{
	if (SkeletalMesh == nullptr)
	{
		UE_LOG(LogArcSkeletalMeshRandomWeights, Warning, TEXT("AssignRandomBoneWeights called with null SkeletalMesh."));
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AssignRandomBoneWeights", "Assign Random Bone Weights"));
	SkeletalMesh->Modify();

	SkeletalMesh->PreEditChange(nullptr);

	const bool bSuccess = ArcSkeletalMeshRandomWeightsUtility_Private::AssignFullCoverageWeightsToLOD(SkeletalMesh, LODIndex);

	SkeletalMesh->PostEditChange();

	if (bSuccess)
	{
		SkeletalMesh->MarkPackageDirty();
	}
}

#undef LOCTEXT_NAMESPACE
