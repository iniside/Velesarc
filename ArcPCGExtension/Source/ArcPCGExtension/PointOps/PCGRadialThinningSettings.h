// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "PCGSettings.h"
#include "PCGElement.h"

#include "Metadata/PCGAttributePropertySelector.h"

#include "PCGRadialThinningSettings.generated.h"

namespace PCGRadialThinningConstants
{
	// `extern` + a .cpp definition, NOT a header-scope `const FName`: the latter gives every including
	// translation unit its own copy plus a load-time FName constructor. This is the engine idiom
	// (PCGCommon.h:389-397 / PCGCommon.cpp:556-558), and the form the sibling nodes use.

	/** Points to thin. The survivors are what the node emits. */
	extern ARCPCGEXTENSION_API const FName PointsLabel;
}

/**
 * Removes points whose per-point exclusion SPHERES overlap, keeping the higher-priority one: point `a`
 * survives iff no already-accepted point `b` satisfies  Dist(a, b) < (Ra + Rb) * OverlapFactor. The
 * graph-level primitive behind "thin this scatter so nothing grows through anything else", where the
 * spacing is driven by a per-point size rather than a single global distance.
 *
 * ------------------------------------------------------------------------------------------------
 * WHY THIS EXISTS NEXT TO UPCGSelfPruningSettings (PCGSelfPruning.h:127) — DO NOT DELETE IT AS A
 * DUPLICATE. The engine node covers ~85% of this and is the right tool most of the time. Two gaps are
 * the entire reason for this one, and both are load-bearing:
 *
 *   1. SPHERE, NOT AABB. Self-pruning tests point BOUNDS against each other with
 *      Octree.FindElementsWithBoundsTest(FBoxCenterAndExtent(...)) (PCGSelfPruning.cpp:149) — an
 *      axis-aligned BOX overlap, with the engine's own `// TODO: check on an oriented-box basis?` on
 *      the next line. Two points offset DIAGONALLY can have overlapping AABBs while their spheres are
 *      comfortably apart, so a box test over-prunes on the diagonal and the result is visibly
 *      anisotropic. This node does the true d < Ra + Rb test.
 *   2. NON-DESTRUCTIVE BOUNDS. The engine's radius is the point's own bounds, so driving it from an
 *      attribute means baking that radius INTO the bounds first (UPCGPointExtentsModifierSettings) —
 *      which corrupts them for every downstream consumer (spawners, collision, bounds-based samplers)
 *      and cannot be undone. This node reads the radius from an attribute or a constant and writes
 *      NOTHING: survivors are copied through byte-for-byte, bounds included.
 *
 * If neither gap matters, use UPCGSelfPruningSettings — it is time-sliced and this node is not.
 * ------------------------------------------------------------------------------------------------
 *
 * EXPLICIT OPT-IN FOR THE ATTRIBUTES. bUseRadiusAttribute and bUsePriorityAttribute default to FALSE,
 * and the selectors are hidden behind them. This is not cosmetic: FPCGAttributePropertyInputSelector
 * default-CONSTRUCTS to @Last (PCGAttributePropertySelector.cpp:511-514), so an untouched selector
 * resolves to whatever the input's last attribute happens to be — a PatchId from Clustered Scatter,
 * say — which, being numeric, would be silently accepted as a radius. With the toggles off no accessor
 * is created and the constant is used unconditionally, so the defaults mean "use the constant". When a
 * toggle IS on and its selector resolves to nothing, the constant is used and ONE graph warning is
 * raised.
 *
 * DETERMINISM. Points are visited in priority order, and PRIORITY TIES ARE BROKEN BY POINT INDEX, never
 * randomly and never by TArray::Sort's (unstable, introsort) incidental order. Survivors are then
 * emitted in ascending INPUT index order, not in visit order, so the output is a subsequence of the
 * input and permuting the input permutes the output the same way.
 *
 * ONE INPUT DATA AT A TIME, one output data per input data: thinning NEVER crosses a data boundary,
 * because each tagged data is its own point set. Union upstream (Merge/Gather) if two sets must thin
 * each other.
 *
 * ACCELERATION is a uniform grid of ACCEPTED points only, rebuilt as the accepted set grows. Neither of
 * the two ready-made structures fits: FArcForestSpatialHash is built once over a fixed, non-owning
 * position view (ArcForestSpatialHash.h:28-78), and the input's FPointOctree indexes ALL points rather
 * than the accepted subset and is unsafe to mutate concurrently (PCGBasePointData.h:454).
 *
 * THREADING: no surface sampling and no octree mutation, so CanExecuteOnlyOnMainThread and IsCacheable
 * stay at their defaults. The accept loop is inherently SERIAL — each decision depends on every
 * decision before it.
 *
 * PASS-THROUGH: points, attributes and tags are copied by ArcPCGScatterCore::PassThroughPoints, the sole
 * authority for that sequence. Nothing is added and nothing is modified.
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), meta = (Keywords = "arc extension thinning prune spacing radius"))
class UPCGRadialThinningSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("RadialThinning")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGRadialThinning", "NodeTitle", "Arc Radial Thinning"); }
	virtual FText GetNodeTooltipText() const override;
	// Filter: this node REMOVES points and changes nothing about the ones it keeps.
	/** PointOps for all five Arc nodes — see the note on UPCGScatterNearPointsSettings::GetType. */
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::PointOps; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }
	virtual FPCGElementPtr CreateElement() const override;

public:
	/** Read the exclusion radius per point from an attribute instead of using Radius Constant Cm. OFF by
	 *  default, and that default is load-bearing — see the EXPLICIT OPT-IN block in the class comment. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Thinning", meta = (PCG_Overridable))
	bool bUseRadiusAttribute = false;

	/** Per-point exclusion radius in cm. Only consulted when Use Radius Attribute is on; falls back to
	 *  Radius Constant Cm — with one graph warning — when it resolves to nothing or to a non-numeric
	 *  attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Thinning", meta = (PCG_Overridable, EditCondition = "bUseRadiusAttribute", EditConditionHides))
	FPCGAttributePropertyInputSelector RadiusAttribute;

	/** Exclusion radius used for every point when Use Radius Attribute is off, or when the selector does
	 *  not resolve. Two points are compared against the SUM of their two radii, so this default of 200
	 *  keeps uniform points 400 cm apart. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Thinning", meta = (ClampMin = "0.0", PCG_Overridable))
	double RadiusConstantCm = 200.0;

	/** Rank points by a separate attribute instead of by their radius. OFF by default, in which case the
	 *  priority of a point IS its radius — so with Largest First on, big points win, which is the usual
	 *  intent. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Thinning", meta = (PCG_Overridable))
	bool bUsePriorityAttribute = false;

	/** Per-point rank. Only consulted when Use Priority Attribute is on; falls back to the radius — with
	 *  one graph warning — when it resolves to nothing or to a non-numeric attribute. Ties are always
	 *  broken by point index. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Thinning", meta = (PCG_Overridable, EditCondition = "bUsePriorityAttribute", EditConditionHides))
	FPCGAttributePropertyInputSelector PriorityAttribute;

	/** Visit points from the HIGHEST priority down, so the highest-priority member of an overlapping pair
	 *  is the one that survives. Turn it off to keep the lowest instead. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Thinning", meta = (PCG_Overridable))
	bool bLargestFirst = true;

	/** Scales the rejection distance: a pair is rejected below (Ra + Rb) * this. 1 means "spheres may
	 *  touch but not overlap"; above 1 forces a gap; 0 disables rejection entirely. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Thinning", meta = (ClampMin = "0.0", PCG_Overridable))
	float OverlapFactor = 1.0f;
};

class FPCGRadialThinningElement : public IPCGElement
{
protected:
	virtual bool SupportsBasePointDataInputs(FPCGContext* InContext) const override { return true; }
	// Single-pass: returns true on the first call, always. No custom context, no PauseUntilNextFrame, no
	// worker task, no AbortInternal. CanExecuteOnlyOnMainThread and IsCacheable stay at their defaults —
	// there is no surface sampling here, so neither the GT pin nor the landscape-CRC gap applies.
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
