// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "PCGSettings.h"
#include "PCGElement.h"

#include "PCGClusteredScatterSettings.generated.h"

namespace PCGClusteredScatterConstants
{
	// `extern` + a .cpp definition, NOT a header-scope `const FName`: the latter gives every including
	// translation unit its own copy plus a load-time FName constructor. This is the engine idiom
	// (PCGCommon.h:389-397 / PCGCommon.cpp:556-558), and the same form its sibling node uses.

	/** Surface the clustered points are projected onto, and whose XY bounds define the scatter region. */
	extern ARCPCGEXTENSION_API const FName SurfaceLabel;
}

/**
 * Generates a CLUSTERED (patchy) point cloud over a surface — a Thomas process: a homogeneous Poisson
 * parent process over the surface's XY bounds, each parent spawning a fixed number of offspring at
 * isotropic Gaussian offsets. The graph-level primitive behind "patchy undergrowth": clumps of a
 * characteristic size, with none of the forest sim's stateful machinery.
 *
 * NO ENGINE EQUIVALENT (checked against the whole PCG Elements inventory): UPCGSurfaceSamplerSettings
 * is a jittered REGULAR GRID with no noise or clustering mode, the only Poisson code in the plugin is
 * private to PCGCreatePointsSphere.cpp and samples a sphere surface, and UPCGClusterSettings is
 * KMeans/EM CLASSIFICATION of points that already exist — the inverse operation, not a generator.
 *
 * MATHS: the parent placement is lifted from FArcForestPatchField::Build (ArcForestPatchField.cpp:78-115)
 * and the offspring offset from the sim's offspring block (ArcForestSimulator.cpp:310-317), both via
 * Scatter/ArcPCGScatterCore.h, which is the sole authority. FArcForestPatchField itself is deliberately
 * NOT instantiated: it is non-copyable and its FArcForestSpatialHash members hold non-owning views into
 * its own arrays (ArcForestPatchField.h:57-61), so it cannot be used as a value here. The maths is
 * lifted; the class is not. What is dropped along the way is the sim's inhomogeneous thinning — there is
 * no suitability field on a standalone node, so the parent process here is homogeneous.
 *
 * SEEDING: this node has NO input points, so there is no position to key a stream on and the parent
 * sequence is inherently serial. It therefore uses the INDEX-derived helper (MakeStreamForIndex) where
 * its sibling Scatter Near Points uses the position-derived one — the chunk-invariance problem that
 * motivates MakeStreamForPosition cannot arise without a chunked point input.
 *
 * THREADING: CanExecuteOnlyOnMainThread -> TRUE. ProjectOntoSurface calls UPCGSpatialData::ProjectPoint,
 * which on landscape can trigger GT-only cache creation (rationale in full at PCGForestSimSettings.h:23-35).
 *
 * CACHING: IsCacheable -> FALSE, for the reason its sibling states (PCGForestSimSettings.h:43-44) —
 * surface changes are not reliably captured in the input CRC, so a cached run would keep serving points
 * projected onto a surface that has since moved. This node samples the same surface through the same
 * call, so the gap applies identically.
 *
 * INVALID BOUNDS ARE A HARD REFUSAL — and that is DELIBERATELY UNLIKE Scatter Near Points, which falls
 * back to each source point's own Z on a surface reporting an invalid FBox (e.g. World Ray Hit Query,
 * PCGWorldData.h:460). This is not an inconsistency between the two nodes: that node keeps its SOURCE
 * POINTS on an unbounded surface and only has to decide a query HEIGHT, whereas this node derives BOTH
 * its query height and its entire XY scatter REGION from the same bounds. Without bounds there is no XY
 * extent to place parents within and no points to key a height from, so there is nothing left to
 * generate. An unbounded surface therefore raises a graph warning and emits nothing; intersect it with a
 * volume, or use a bounded surface, upstream.
 *
 * MISSES ARE DROPPED: an offspring whose projection misses the surface produces NO point. There is
 * deliberately no fallback Z — a point floating at an arbitrary height is worse than a missing one. The
 * realised count is therefore at or below NumParents * PointsPerParent.
 *
 * BUDGET: MaxPoints binds BEFORE anything is generated, by reducing the parent count, so a clamped run
 * costs the clamped amount rather than generating and then truncating. Whole patches are dropped rather
 * than a patch being cut in half. It binds with a graph warning — at the default density a very large
 * landscape would otherwise silently emit hundreds of thousands of points.
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), meta = (Keywords = "arc extension scatter cluster patch thomas"))
class UPCGClusteredScatterSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("ClusteredScatter")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGClusteredScatter", "NodeTitle", "Arc Clustered Scatter"); }
	virtual FText GetNodeTooltipText() const override;
	// Sampler: it projects onto a surface, and its output extent is the surface's, not an authored shape.
	/** PointOps for all five Arc nodes — see the note on UPCGScatterNearPointsSettings::GetType. */
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::PointOps; }
#endif

	// Uses the inherited UPCGSettings::Seed (PCGSettings.h:490). A second `Seed` UPROPERTY cannot be
	// declared — UHT rejects redeclaring an inherited property name, which is why the sibling sim had to
	// call its own SimSeed. UseSeed() is what un-hides the inherited field in the details panel
	// (precedents: PCGCreatePointsSphere.h:53, PCGAttributeNoise.h:54).
	virtual bool UseSeed() const override { return true; }

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }
	virtual FPCGElementPtr CreateElement() const override;

public:
	/** Patch (parent) density per square metre of the surface's XY footprint. The parent count is
	 *  round(ParentDensityPerM2 * AreaM2), then reduced if the MaxPoints budget binds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Scatter", meta = (ClampMin = "0.0", PCG_Overridable))
	double ParentDensityPerM2 = 0.001;

	/** Standard deviation, in cm, of the isotropic Gaussian offset from a patch centre — i.e. the patch
	 *  size. Must be above zero; a degenerate sigma collapses every child onto its parent and is refused. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Scatter", meta = (ClampMin = "0.0", PCG_Overridable))
	double PatchRadiusSigmaCm = 400.0;

	/** Offspring generated per patch. Each one that misses the surface yields no point, so the realised
	 *  count is at or below NumParents * PointsPerParent. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Scatter", meta = (ClampMin = "0", PCG_Overridable))
	int32 PointsPerParent = 8;

	/**
	 * Half-extents written to every emitted point's bounds, in centimetres. Matches the engine Surface
	 * Sampler's PointExtents default, so a point is a 1 m box.
	 *
	 * This is what the debug display sizes its boxes from (ScaleMethod defaults to Extents), and what
	 * downstream spawners and Arc Radial Thinning read. Leaving it tiny makes debug boxes invisible.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Clustered Scatter", meta = (PCG_Overridable))
	FVector PointExtents = FVector(50.0);

	/** Hard upper bound on the generated point count. It binds by REDUCING the parent count before
	 *  anything is generated, and raises a graph warning when it does. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Scatter", meta = (ClampMin = "0", PCG_Overridable))
	int32 MaxPoints = 100000;

	/** Write the index of the patch each point belongs to, as an int32 attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Scatter", meta = (PCG_Overridable))
	bool bWritePatchIdAttribute = true;

	/** Name of the patch-id attribute. Ids are dense parent indices in [0, NumParents). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Scatter", meta = (EditCondition = "bWritePatchIdAttribute", PCG_Overridable))
	FName PatchIdAttributeName = TEXT("PatchId");
};

class FPCGClusteredScatterElement : public IPCGElement
{
public:
	// ProjectPoint is not worker-safe on landscape (GT-only cache creation) — pin to the game thread.
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* InContext) const override { return true; }
	// Surface changes are not reliably captured in the input CRC — same call, same gap as the sim.
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool SupportsBasePointDataInputs(FPCGContext* InContext) const override { return true; }
	// Single-pass: returns true on the first call, always. No custom context, no PauseUntilNextFrame,
	// no worker task, no AbortInternal — none of the sim's state machine is needed or wanted here.
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
