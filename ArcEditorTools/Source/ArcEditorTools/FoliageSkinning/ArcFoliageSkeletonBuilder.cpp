// Copyright Lukasz Baran. All Rights Reserved.

#include "FoliageSkinning/ArcFoliageSkeletonBuilder.h"

#include "Clustering/KMeans.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Math/RotationMatrix.h"
#include "Operations/SkinWeightBinding.h"
#include "PCA3.h"
#include "ReferenceSkeleton.h"
#include "Selections/MeshConnectedComponents.h"
#include "SkeletalMeshAttributes.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcFoliageSkeletonBuilder, Log, All);

namespace ArcFoliageSkeletonBuilderCpp
{
	using UE::Geometry::FClusterKMeans;
	using UE::Geometry::FComputePCA3Options;
	using UE::Geometry::FDynamicMesh3;
	using UE::Geometry::FMeshConnectedComponents;
	using UE::Geometry::FPCA3d;

	/** One grass clump: a set of mesh vertex IDs skinned to its own bone chain. */
	struct FClump
	{
		TArray<int32> VertexIDs;
	};

	/** Straight bone chain description for one clump, derived from PCA of its vertices. */
	struct FClumpChain
	{
		/** Unit blade axis, sign-flipped so it points from the base upward (base = low-Z end). */
		FVector3d Axis = FVector3d::UnitZ();

		/** Chain start point (on the axis line through the clump centroid, at the minimum projection). */
		FVector3d BasePoint = FVector3d::ZeroVector;

		/** Vertex spread along the axis (max - min projection). */
		double Extent = 0.0;

		/** Number of bones in the chain. */
		int32 NumBones = 1;
	};

	/**
	 * Group connected vertex islands into clumps. Each island is its own clump unless there are more
	 * islands than MaxClumps, in which case island centroids are KMeans-clustered and the islands of
	 * each cluster merge into one clump.
	 */
	void BuildClumps(const FDynamicMesh3& Mesh, const int32 MaxClumps, TArray<FClump>& OutClumps)
	{
		FMeshConnectedComponents Components(&Mesh);
		Components.FindConnectedVertices();

		const int32 NumIslands = Components.Num();
		OutClumps.Reset();

		if (NumIslands <= MaxClumps)
		{
			OutClumps.SetNum(NumIslands);
			for (int32 IslandIndex = 0; IslandIndex < NumIslands; ++IslandIndex)
			{
				OutClumps[IslandIndex].VertexIDs = Components.GetComponent(IslandIndex).Indices;
			}
			return;
		}

		// Too many islands: merge them by KMeans-clustering their centroids.
		TArray<FVector> Centroids;
		Centroids.Reserve(NumIslands);
		for (int32 IslandIndex = 0; IslandIndex < NumIslands; ++IslandIndex)
		{
			FVector3d Sum = FVector3d::ZeroVector;
			const TArray<int32>& IslandVerts = Components.GetComponent(IslandIndex).Indices;
			for (const int32 VertexID : IslandVerts)
			{
				Sum += Mesh.GetVertex(VertexID);
			}
			Centroids.Add(Sum / (double)FMath::Max(1, IslandVerts.Num()));
		}

		FClusterKMeans KMeans;
		const int32 NumClusters = KMeans.ComputeClusters<FVector>(Centroids, MaxClumps);

		OutClumps.SetNum(NumClusters);
		for (int32 IslandIndex = 0; IslandIndex < NumIslands; ++IslandIndex)
		{
			const int32 ClusterID = KMeans.ClusterIDs[IslandIndex];
			OutClumps[ClusterID].VertexIDs.Append(Components.GetComponent(IslandIndex).Indices);
		}

		UE_LOG(LogArcFoliageSkeletonBuilder, Log,
			TEXT("Merged %d vertex islands into %d clumps (MaxClumps=%d)."), NumIslands, NumClusters, MaxClumps);
	}

	/** PCA blade axis + base point + bone count for one clump. */
	FClumpChain ComputeClumpChain(
		const FDynamicMesh3& Mesh, const FClump& Clump, const float BoneSpacing, const int32 MinBonesPerClump,
		const int32 MaxBonesPerClump)
	{
		FClumpChain Chain;

		TArray<FVector3d> Positions;
		Positions.Reserve(Clump.VertexIDs.Num());
		for (const int32 VertexID : Clump.VertexIDs)
		{
			Positions.Add(Mesh.GetVertex(VertexID));
		}

		// Dominant PCA eigenvector = blade axis. On unusable PCA (too few points / degenerate cloud)
		// keep the +Z fallback so single-vertex or flat-disc clumps still get a valid vertical chain.
		FComputePCA3Options PcaOptions;
		PcaOptions.bScaleDataToUnitCube = false;
		FPCA3d Pca;
		if (Pca.Compute(Positions, PcaOptions))
		{
			FVector3d Dominant = Pca.Eigenvectors[0];
			if (Dominant.Normalize(UE_DOUBLE_SMALL_NUMBER))
			{
				Chain.Axis = Dominant;
			}
		}

		// Sign-flip so the low-Z end of the clump is the chain base (grass grows up).
		if (Chain.Axis.Z < 0.0)
		{
			Chain.Axis = -Chain.Axis;
		}

		// A near-horizontal dominant axis means the clump is a merged ground patch (or a wide card) whose
		// width beats its height in the PCA — a chain along it would lie flat and bend the grass sideways.
		// Grass deforms about vertical, so fall back to +Z and let the projection loop below re-measure.
		if (Chain.Axis.Z < 0.5)
		{
			Chain.Axis = FVector3d::UnitZ();
		}

		FVector3d Centroid = FVector3d::ZeroVector;
		double MinProjection = TNumericLimits<double>::Max();
		double MaxProjection = -TNumericLimits<double>::Max();
		for (const FVector3d& Position : Positions)
		{
			Centroid += Position;
			const double Projection = FVector3d::DotProduct(Position, Chain.Axis);
			MinProjection = FMath::Min(MinProjection, Projection);
			MaxProjection = FMath::Max(MaxProjection, Projection);
		}
		Centroid /= (double)FMath::Max(1, Positions.Num());

		Chain.Extent = FMath::Max(0.0, MaxProjection - MinProjection);
		// Base point on the axis line through the centroid, at the minimum projection.
		Chain.BasePoint = Centroid + Chain.Axis * (MinProjection - FVector3d::DotProduct(Centroid, Chain.Axis));

		// Bone-count heuristic: one bone per BoneSpacing of extent (rounded up), clamped. The MinBones floor
		// guarantees short clumps still get a bendable chain instead of one rigid bone.
		const int32 MinBones = FMath::Max(1, MinBonesPerClump);
		const int32 MaxBones = FMath::Max(MinBones, MaxBonesPerClump);
		Chain.NumBones = MinBones;
		if (BoneSpacing > UE_SMALL_NUMBER)
		{
			Chain.NumBones = FMath::Clamp(FMath::CeilToInt32(Chain.Extent / (double)BoneSpacing), MinBones, MaxBones);
		}
		return Chain;
	}
}

namespace Arc::FoliageSkeleton
{
	bool BuildClumpSkeletonAndBind(
		UE::Geometry::FDynamicMesh3& InOutMesh,
		const FArcSkeletonBuildParams& Params,
		FReferenceSkeleton& OutRefSkeleton,
		FArcSkeletonBuildStats& OutStats)
	{
		using namespace ArcFoliageSkeletonBuilderCpp;
		namespace SkinBinding = UE::Geometry::SkinBinding;

		OutStats = FArcSkeletonBuildStats();
		OutRefSkeleton = FReferenceSkeleton();

		if (InOutMesh.VertexCount() == 0)
		{
			UE_LOG(LogArcFoliageSkeletonBuilder, Warning, TEXT("BuildClumpSkeletonAndBind: mesh has no vertices."));
			return false;
		}

		TArray<FClump> Clumps;
		BuildClumps(InOutMesh, FMath::Max(1, Params.MaxClumps), Clumps);
		OutStats.ClumpCount = Clumps.Num();

		// Build the reference skeleton and the FBonePoseInfo hierarchy TOGETHER, one Add per bone, so the
		// FBonePoseInfo array order is exactly the RefSkeleton raw bone order (skin-weight bone indices are
		// hierarchy-array indices and must line up with the skeleton downstream).
		TArray<SkinBinding::FBonePoseInfo> Hierarchy;
		TArray<TArray<int32, TInlineAllocator<16>>> BoneGroups;
		BoneGroups.Reserve(Clumps.Num() + 1);
		{
			FReferenceSkeletonModifier Modifier(OutRefSkeleton, nullptr);

			// Root at origin. Excluded from every clump group: its bone fan spans the whole
			// mesh and would steal weight mid-blade.
			const FName RootName(TEXT("root"));
			Modifier.Add(FMeshBoneInfo(RootName, RootName.ToString(), INDEX_NONE), FTransform::Identity);
			Hierarchy.Add(SkinBinding::FBonePoseInfo{FTransform::Identity, RootName, INDEX_NONE});
			const int32 RootBoneIndex = 0;

			for (int32 ClumpIndex = 0; ClumpIndex < Clumps.Num(); ++ClumpIndex)
			{
				const FClumpChain Chain = ComputeClumpChain(
					InOutMesh, Clumps[ClumpIndex], Params.BoneSpacing, Params.MinBonesPerClump,
					Params.MaxBonesPerClump);

				// Bone rotation aligns +X (UE bone convention) to the clump axis; the whole straight
				// chain shares it, so child locals are pure +X translations.
				const FQuat BoneRotation = FRotationMatrix::MakeFromX((FVector)Chain.Axis).ToQuat();
				const double StepLength = Chain.Extent / (double)Chain.NumBones;

				TArray<int32, TInlineAllocator<16>>& ClumpGroup = BoneGroups.AddDefaulted_GetRef();
				int32 ParentBoneIndex = RootBoneIndex;
				for (int32 BoneInChain = 0; BoneInChain < Chain.NumBones; ++BoneInChain)
				{
					const FName BoneName(FString::Printf(TEXT("clump%d_%d"), ClumpIndex, BoneInChain));

					// Parent-LOCAL transform: first bone maps from the identity root (= component space);
					// later bones step +X in the parent's frame.
					FTransform LocalTransform;
					if (BoneInChain == 0)
					{
						LocalTransform = FTransform(BoneRotation, (FVector)Chain.BasePoint);
					}
					else
					{
						LocalTransform = FTransform(FQuat::Identity, FVector(StepLength, 0.0, 0.0));
					}

					Modifier.Add(FMeshBoneInfo(BoneName, BoneName.ToString(), ParentBoneIndex), LocalTransform);
					const int32 BoneIndex =
						Hierarchy.Add(SkinBinding::FBonePoseInfo{LocalTransform, BoneName, ParentBoneIndex});
					ClumpGroup.Add(BoneIndex);
					ParentBoneIndex = BoneIndex;
				}
			}

			// One extra root-only group: the default for any vertex not claimed by a clump (and the
			// harmless value behind invalid VIDs on non-compact meshes).
			TArray<int32, TInlineAllocator<16>>& RootGroup = BoneGroups.AddDefaulted_GetRef();
			RootGroup.Add(RootBoneIndex);
		} // Modifier destructor rebuilds the reference skeleton's derived data.

		OutStats.BoneCount = Hierarchy.Num();

		// Vertex -> bone-group map. MUST be sized MaxVertexID(): the binding API silently falls back to an
		// unconstrained all-bones bind on size mismatch, and INDEX_NONE entries search all bones. Default every
		// slot to the root-only group, then stamp each clump's vertices — leaving every valid VID assigned.
		const int32 RootGroupIndex = BoneGroups.Num() - 1;
		TArray<int32> VIDtoGroupIndex;
		VIDtoGroupIndex.Init(RootGroupIndex, InOutMesh.MaxVertexID());
		for (int32 ClumpIndex = 0; ClumpIndex < Clumps.Num(); ++ClumpIndex)
		{
			for (const int32 VertexID : Clumps[ClumpIndex].VertexIDs)
			{
				VIDtoGroupIndex[VertexID] = ClumpIndex;
			}
		}

		if (!InOutMesh.HasAttributes())
		{
			InOutMesh.EnableAttributes();
		}

		SkinBinding::FBindSettings BindSettings;
		BindSettings.Stiffness = Params.BindStiffness;
		BindSettings.MaxInfluences = FMath::Max(1, Params.MaxInfluences);
		BindSettings.VoxelResolution = 256;
		BindSettings.BindType = UE::Geometry::ESkinBindingType::DirectDistance;

		// Also copies the bone hierarchy onto the mesh bone attributes (CopyBonesToMesh), which the
		// skeletal-mesh conversion downstream validates against.
		SkinBinding::CreateConstrainedSkinWeights(
			InOutMesh, Hierarchy, FSkeletalMeshAttributes::DefaultSkinWeightProfileName, BindSettings,
			BoneGroups, VIDtoGroupIndex);

		UE_LOG(LogArcFoliageSkeletonBuilder, Log,
			TEXT("Built clump skeleton: %d clumps, %d bones (root included), %d vertices bound."),
			OutStats.ClumpCount, OutStats.BoneCount, InOutMesh.VertexCount());
		return true;
	}
}
