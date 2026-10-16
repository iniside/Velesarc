// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ArcTerrainCollarBake.generated.h"

class UStaticMesh;

/** Cost and shape of the collar blend. All lengths are terrain-space centimetres. */
USTRUCT()
struct FArcTerrainCollarParams
{
	GENERATED_BODY()

	/** Radius of the smooth-min blend between the mesh and the ground. */
	UPROPERTY(EditAnywhere, Category = "Collar", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float CollarRadiusCm = 30.0f;

	/** Marching-cubes cell size — tessellation of the result, not the fillet's accuracy. */
	UPROPERTY(EditAnywhere, Category = "Collar", meta = (ClampMin = "0.5", ForceUnits = "cm"))
	float CollarTexelCm = 3.0f;

	/** Mesh SDF cell size — the fillet's accuracy, and the whole memory cost of the bake. */
	UPROPERTY(EditAnywhere, Category = "Collar", meta = (ClampMin = "0.5", ForceUnits = "cm"))
	float CollarSdfCellCm = 15.0f;

	/** A triangle survives only where the blend pulls the surface at least this far off both inputs. */
	UPROPERTY(EditAnywhere, Category = "Collar", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float CollarTrimCm = 0.2f;

	/** Outward offset that keeps the strip off the terrain and off the rock. */
	UPROPERTY(EditAnywhere, Category = "Collar", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float CollarBiasCm = 1.0f;
};

/** Baked collar strip. Serializable, cooks, and is rendered under the modifier's own transform. */
USTRUCT()
struct FArcTerrainCollarMesh
{
	GENERATED_BODY()

	/** Component space. */
	UPROPERTY()
	TArray<FVector3f> Positions;

	/** Triples into Positions. */
	UPROPERTY()
	TArray<int32> Indices;

	/** Component space, one per position. */
	UPROPERTY()
	TArray<FVector3f> Normals;

	/** Planar from the footprint's terrain XY, one per position. */
	UPROPERTY()
	TArray<FVector2f> UVs;

	/** Digests the mesh, the terrain height texels the bake ran against, the pose and the parameters. */
	UPROPERTY()
	FGuid ContentId;

	bool IsValid() const
	{
		return Positions.Num() >= 3
			&& Indices.Num() >= 3
			&& Indices.Num() % 3 == 0
			&& Normals.Num() == Positions.Num()
			&& UVs.Num() == Positions.Num()
			&& ContentId.IsValid();
	}

	void Reset()
	{
		Positions.Reset();
		Indices.Reset();
		Normals.Reset();
		UVs.Reset();
		ContentId.Invalidate();
	}
};

/** Terrain-local composited heights over an inclusive texel rectangle — the output of
 *  UArcTerrainSubsystem::SampleTerrainHeightTexels, including its bComplete flag. */
struct FArcTerrainCollarHeightField
{
	/** Row-major, Dims.X inner; sample (0, 0) sits at MinTexel * TexelSizeCm. */
	TArray<float> Heights;

	FIntPoint MinTexel = FIntPoint::ZeroValue;

	FIntPoint Dims = FIntPoint::ZeroValue;

	float TexelSizeCm = 0.0f;

	bool bComplete = false;

	bool IsValid() const
	{
		return Dims.X >= 2 && Dims.Y >= 2 && TexelSizeCm > 0.0f && FMath::IsFinite(TexelSizeCm)
			&& static_cast<int64>(Dims.X) * static_cast<int64>(Dims.Y) == static_cast<int64>(Heights.Num());
	}
};

namespace ArcTerrainCollarBake
{
#if WITH_EDITOR
	/** Invalid when Mesh has no CPU-readable LOD0 or HeightField is malformed. */
	ARCTERRAIN_API FGuid ComputeContentId(const UStaticMesh& Mesh, const FTransform& ComponentToTerrain,
		const FArcTerrainCollarHeightField& HeightField, const FArcTerrainCollarParams& Params);

	/**
	 * Meshes smoothmin(mesh SDF, terrain height field) over the contact band and writes the trimmed,
	 * biased strip to OutCollar. Refuses — logs, empties OutCollar and returns false, never a silent
	 * empty success — for an incomplete height field, an unsupported pose, a mesh whose LOD0 is not
	 * CPU-readable, an SDF that does not fit the cell budget at any cell size, or an empty result.
	 */
	ARCTERRAIN_API bool Bake(const UStaticMesh& Mesh, const FTransform& ComponentToTerrain,
		const FArcTerrainCollarHeightField& HeightField, const FArcTerrainCollarParams& Params,
		const FString& DebugName, FArcTerrainCollarMesh& OutCollar);
#endif
}
