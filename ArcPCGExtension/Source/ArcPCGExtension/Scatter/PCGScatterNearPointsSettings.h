// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "PCGSettings.h"
#include "PCGElement.h"

#include "PCGScatterNearPointsSettings.generated.h"

namespace PCGScatterNearPointsConstants
{
	// `extern` + a .cpp definition, NOT a header-scope `const FName`: the latter gives every including
	// translation unit its own copy plus a load-time FName constructor. This is the engine idiom
	// (PCGCommon.h:389-397 / PCGCommon.cpp:556-558).

	/** Surface the darts are projected onto. */
	extern ARCPCGEXTENSION_API const FName SurfaceLabel;

	/** Source points the annulus is centred on. */
	extern ARCPCGEXTENSION_API const FName PointsLabel;
}

/**
 * Scatters N new points in an area-uniform annulus [MinRadiusCm, MaxRadiusCm] around every source
 * point, projected onto a surface. The graph-level primitive behind "spawn ferns near trees":
 * a single generation of proximity scatter, with none of the forest sim's stateful machinery.
 *
 * NO ENGINE EQUIVALENT (checked against the whole PCG Elements inventory): UPCGCopyPointsSettings
 * copies a FIXED pattern with no radius or randomness, UPCGTransformPointsSettings jitters inside a
 * BOX and cannot create points, and UPCGCreatePointsSphereSettings is a sphere SURFACE at one fixed
 * origin. Nothing samples an annulus around each of N sources.
 *
 * SAMPLING: annulus and RNG both come from Scatter/ArcPCGScatterCore.h, which is the sole authority
 * for the maths — nothing here re-derives it. The per-source stream is POSITION-derived
 * (MakeStreamForPosition), never index-derived: point indices restart at 0 in every tagged data, so
 * an index-derived stream would make the output depend on how the input happens to be chunked
 * upstream. Position-derived, it does not. The cost of that property: PCGHelpers::ComputeSeedFromPosition
 * TRUNCATES each axis to integer centimetres (PCGHelpers.cpp:50-53), so two source points inside the
 * same 1 cm^3 cell share a stream and scatter identically on top of one another.
 *
 * THREADING: CanExecuteOnlyOnMainThread -> TRUE. ProjectOntoSurface calls UPCGSpatialData::ProjectPoint,
 * which on landscape can trigger GT-only cache creation (rationale in full at PCGForestSimSettings.h:23-35).
 *
 * CACHING: IsCacheable -> FALSE, for the reason its sibling states (PCGForestSimSettings.h:43-44) —
 * surface changes are not reliably captured in the input CRC, so a cached run would keep serving
 * points projected onto a surface that has since moved. This node samples the same surface through
 * the same call, so the gap applies identically.
 *
 * QUERY HEIGHT: darts drop from the top of the surface's bounds. An UNBOUNDED surface reports an
 * INVALID FBox — UPCGWorldRayHitData, the World Ray Hit Query node and the idiomatic "project onto
 * world collision" source, hardcodes FBox(EForceInit::ForceInit) as its bounds (PCGWorldData.h:460,
 * assigned from the ForceInit argument at PCGWorldQuery.cpp:230) — and its Max.Z is then the
 * zero-initialised default, which would drop every dart from Z=0. For those surfaces the query
 * height is EACH SOURCE POINT'S OWN Z, and a graph warning is raised once.
 *
 * There is deliberately NO height knob for that case, because on an unbounded surface this node does
 * not get to choose the trace origin at all:
 *   - UPCGWorldRayHitData overrides SamplePoint but NOT ProjectPoint (PCGWorldData.h:438-448), and
 *     neither does UPCGSurfaceData (PCGSurfaceData.h:29-32), so the base UPCGSpatialData::ProjectPoint
 *     runs (PCGSpatialData.cpp:258-264) and falls through to SamplePoint. There the incoming location
 *     is ORTHOGONALLY PROJECTED onto the plane through QueryParams.RayOrigin with normal RayDirection
 *     before the sweep starts (PCGWorldData.cpp:447-448). Only our X and Y survive; the height along
 *     the ray is the query params' own, defaulted from the source component's transform and local
 *     bounds (PCGWorldQuery.cpp:199-211). Any Z this node passed — 0 or 1e9 — yields bit-identical
 *     output, so a height property here would be an inert knob wired to misleading guidance.
 *   - UPCGWorldVolumetricData likewise leaves ProjectPoint unoverridden (commented out at
 *     PCGWorldData.h:363), and its SamplePoint is an OVERLAP TEST AT THE QUERY LOCATION
 *     (PCGWorldData.cpp:241-266), not a downward trace — so an arbitrary upward offset would there
 *     guarantee a miss rather than a hit.
 * The source's own Z is therefore the only value that is either meaningful (volumetric) or harmless
 * (ray hit). When points are missing on an unbounded surface, the fix belongs on the SOURCE node's
 * parameters (World Ray Hit Query's ray origin / direction / length), not on this node.
 *
 * MISSES ARE DROPPED: a dart that lands off the surface produces NO point. There is deliberately no
 * fallback Z — a point floating at an arbitrary height is worse than a missing one. This is a
 * SEPARATE matter from the query-height fallback above: that one picks where the query STARTS, a
 * miss is still a miss and still yields nothing.
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), meta = (Keywords = "arc extension scatter annulus"))
class UPCGScatterNearPointsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("ScatterNearPoints")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGScatterNearPoints", "NodeTitle", "Arc Scatter Near Points"); }
	virtual FText GetNodeTooltipText() const override;
	// Sampler: this projects onto a surface, it does not create points from nothing.
	/**
	 * PointOps for ALL FIVE Arc nodes, deliberately, even though this one is a generator: the palette
	 * category is not something a node can name — the schema reads it straight off the EPCGSettingsType
	 * enum (PCGEditorGraphSchema.cpp:575), so sharing one value is the only way to get them adjacent.
	 * The "Arc " title prefix then sorts them together at the top of that category.
	 */
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::PointOps; }
#endif

	// Uses the inherited UPCGSettings::Seed. A second `Seed` UPROPERTY cannot be declared — UHT
	// rejects redeclaring an inherited property name, which is why the sibling sim had to call its
	// own SimSeed. UseSeed() is what un-hides the inherited field in the details panel.
	virtual bool UseSeed() const override { return true; }

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }
	virtual FPCGElementPtr CreateElement() const override;

public:
	/** Inner radius of the annulus in cm. Nothing is scattered closer to a source point than this. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Scatter", meta = (ClampMin = "0.0", PCG_Overridable))
	double MinRadiusCm = 100.0;

	/** Outer radius of the annulus in cm. Clamped UP to MinRadiusCm if authored below it (a degenerate
	 *  ring, never a silently inverted annulus). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Scatter", meta = (ClampMin = "0.0", PCG_Overridable))
	double MaxRadiusCm = 500.0;

	/** Darts thrown per source point. Each dart that misses the surface yields no point, so the
	 *  realised count is at or below SourceCount * PointsPerSource. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Scatter", meta = (ClampMin = "0", PCG_Overridable))
	int32 PointsPerSource = 4;

	/**
	 * Half-extents written to every emitted point's bounds, in centimetres. Matches the engine Surface
	 * Sampler's PointExtents default, so a point is a 1 m box.
	 *
	 * This is what the debug display sizes its boxes from (ScaleMethod defaults to Extents), and what
	 * downstream spawners and Arc Radial Thinning read. Leaving it tiny makes debug boxes invisible.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Scatter", meta = (PCG_Overridable))
	FVector PointExtents = FVector(50.0);

	/** Write the index of the source point each scattered point came from, as an int32 attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Scatter", meta = (PCG_Overridable))
	bool bWriteParentIndexAttribute = true;

	/** Name of the parent-index attribute. Indices are per input data, matching the source point's
	 *  index within the tagged data it came from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Scatter", meta = (EditCondition = "bWriteParentIndexAttribute", PCG_Overridable))
	FName ParentIndexAttributeName = TEXT("ParentIndex");
};

class FPCGScatterNearPointsElement : public IPCGElement
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
