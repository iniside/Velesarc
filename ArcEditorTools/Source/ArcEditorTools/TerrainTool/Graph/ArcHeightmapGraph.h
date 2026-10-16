// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "IO/IoHash.h"
#include "Spatial/SampledScalarField2.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcHeightmapGraph.generated.h"

/**
 * Shared evaluation context handed to every heightmap node during graph evaluation.
 *
 * Plain C++ struct (NOT a USTRUCT) — it is a transient per-evaluation descriptor of the
 * output grid (resolution, footprint coverage in component-local cm, grid origin / cell
 * dimensions). Nodes use AllocateField() to obtain a correctly-sized + positioned output
 * field so that every node in a graph produces grids that line up cell-for-cell.
 *
 * The grid geometry mirrors the no-skirt erosion convention: CellDimensions = Coverage /
 * (Res - 1), GridOrigin = -Coverage * 0.5. A FArcHeightmapNode_Erosion configured with
 * SkirtSize = 0 and GridRes = Res therefore produces a grid identical to this context's,
 * so its FParams::InitialHeight lines up with the upstream node's output.
 */
struct FArcHeightmapEvalContext
{
	int32 ResX = 0;
	int32 ResY = 0;
	double CoverageX = 0.0;
	double CoverageY = 0.0;
	FVector2D GridOrigin = FVector2D::ZeroVector;
	FVector2D CellDimensions = FVector2D::ZeroVector;

	/**
	 * Single source of the grid convention. Builds a context from a footprint coverage (component-local
	 * cm) and a square resolution: ResX = ResY = Max(2, Resolution), grid centred on the origin
	 * (GridOrigin = -Coverage * 0.5), CellDimensions = Coverage / (Res - 1). The Step-4 modifier and any
	 * test MUST build the context through this factory so every consumer shares one grid definition.
	 */
	static FArcHeightmapEvalContext FromCoverage(double CoverageX, double CoverageY, int32 Resolution)
	{
		const int32 Res = FMath::Max(2, Resolution);

		FArcHeightmapEvalContext Ctx;
		Ctx.ResX = Res;
		Ctx.ResY = Res;
		Ctx.CoverageX = CoverageX;
		Ctx.CoverageY = CoverageY;
		Ctx.GridOrigin = FVector2D(-CoverageX * 0.5, -CoverageY * 0.5);
		Ctx.CellDimensions = FVector2D(
			CoverageX / static_cast<double>(Res - 1),
			CoverageY / static_cast<double>(Res - 1));
		return Ctx;
	}

	/** Resize Out to ResX x ResY (init 0), position it at GridOrigin, and set its cell dimensions. */
	void AllocateField(UE::Geometry::FSampledScalarField2f& Out) const
	{
		const int32 W = FMath::Max(2, ResX);
		const int32 H = FMath::Max(2, ResY);
		Out.Resize(W, H, 0.0f);
		Out.SetPosition(UE::Math::TVector2<float>(
			static_cast<float>(GridOrigin.X), static_cast<float>(GridOrigin.Y)));
		Out.CellDimensions = UE::Math::TVector2<float>(
			static_cast<float>(CellDimensions.X), static_cast<float>(CellDimensions.Y));
	}
};

/**
 * Raster-only baked heightmap: a flat float buffer + grid dimensions. No spatial metadata
 * (GridOrigin / CellDimensions) — those are reconstructed from Dims at ToField() time using
 * the normalized [-0.5, +0.5] convention so the layout is independent of coverage.
 */
USTRUCT()
struct FArcBakedHeightField
{
    GENERATED_BODY()

    UPROPERTY() TArray<float> Values;                 // row-major, Dims.X inner
    UPROPERTY() FIntPoint Dims = FIntPoint::ZeroValue;

    bool IsValid() const { return Dims.X >= 2 && Dims.Y >= 2 && Values.Num() == Dims.X * Dims.Y; }
    void Reset() { Values.Reset(); Dims = FIntPoint::ZeroValue; }
};

namespace ArcHeightmapGraph
{
    /** Capture the scalar field's value buffer + dimensions into a FArcBakedHeightField.
     *  GridOrigin and CellDimensions are intentionally discarded — not stored. */
    void FromField(const UE::Geometry::FSampledScalarField2f& In, FArcBakedHeightField& Out);

    /** Reconstruct a FSampledScalarField2f from a baked field using a normalized
     *  [-0.5, +0.5] origin and uniform cell dimensions derived from Dims. */
    void ToField(const FArcBakedHeightField& Baked, UE::Geometry::FSampledScalarField2f& Out);
}

/**
 * Editor-only asset describing a bespoke CPU heightmap-generation graph.
 *
 * One unordered list of polymorphic nodes (FInstancedStruct of FArcHeightmapNode subtypes).
 * Each node carries its own NodeId (assigned in editor) and a list of upstream InputNodeIds;
 * the topological evaluator (Step 3) resolves the DAG and feeds each node its resolved inputs.
 * This header must NOT be included from a runtime module — it pulls in UnrealEd-adjacent
 * editor-only construction.
 */
UCLASS()
class ARCEDITORTOOLS_API UArcHeightmapGraph : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Graph",
		meta = (BaseStruct = "/Script/ArcEditorTools.ArcHeightmapNode", ExcludeBaseStruct))
	TArray<FInstancedStruct> Nodes;

	UPROPERTY(EditAnywhere, Category = "Graph")
	int32 DefaultResolution = 257;

	/** Footprint (cm) used only by the editor's Evaluate Preview. Matches the Heightmap Stamp modifier's
	 *  default UnscaledCoverage so the preview reflects what the modifier produces. Not hashed (ComputeContentHash
	 *  iterates only Nodes), so it never perturbs the stamp cache. */
	UPROPERTY(EditAnywhere, Category = "Graph", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	double PreviewCoverage = 10000.0;

	/** Baked heightmap stored on the graph asset (raster-only, no spatial metadata). */
	UPROPERTY()
	FArcBakedHeightField BakedField;

	/** Baked weight-map layers keyed by Output node LayerName (raster-only, same Dims as BakedField). */
	UPROPERTY()
	TMap<FName, FArcBakedHeightField> BakedLayers;

	/** Identity of the current bake; feed this into the DDC cache key to invalidate on re-bake. */
	UPROPERTY(VisibleAnywhere, Category = "Graph")
	FGuid BakedFieldId;

	/**
	 * Editor-graph node positions keyed by NodeId (editor XY in slate units).
	 * Deliberately NOT EditAnywhere — managed entirely by the editor graph tool.
	 * Excluded from ComputeContentHash (which iterates only Nodes) so dragging a
	 * node never perturbs the stamp cache.
	 */
	UPROPERTY()
	TMap<FGuid, FIntPoint> NodePositions;

	FIntPoint GetNodePosition(const FGuid& NodeId) const
	{
		const FIntPoint* Found = NodePositions.Find(NodeId);
		return Found ? *Found : FIntPoint::ZeroValue;
	}

	void SetNodePosition(const FGuid& NodeId, FIntPoint Position)
	{
		NodePositions.Add(NodeId, Position);
	}

	/** Deterministic Blake3 hash of all node content (UPROPERTY-aware serialization, no struct memcpy). */
	FIoHash ComputeContentHash() const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
