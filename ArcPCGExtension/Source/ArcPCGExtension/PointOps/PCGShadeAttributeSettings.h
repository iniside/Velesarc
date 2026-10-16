// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "PCGSettings.h"
#include "PCGElement.h"

#include "Metadata/PCGAttributePropertySelector.h"
#include "Scatter/ArcPCGScatterCore.h"

#include "PCGShadeAttributeSettings.generated.h"

namespace PCGShadeAttributeConstants
{
	// `extern` + a .cpp definition, NOT a header-scope `const FName`: the latter gives every including
	// translation unit its own copy plus a load-time FName constructor. This is the engine idiom
	// (PCGCommon.h:389-397 / PCGCommon.cpp:556-558), and the form the sibling nodes use.

	/** Points that RECEIVE the shade. These are the points passed through and decorated. */
	extern ARCPCGEXTENSION_API const FName PointsLabel;

	/** Points that CAST the shade (canopy). Optional — an unconnected pin means no occluders at all. */
	extern ARCPCGEXTENSION_API const FName OccludersLabel;
}

/**
 * Writes a per-point TRANSMITTANCE attribute onto the Points input from an accumulated, per-occluder
 * canopy on the Occluders input. The graph-level primitive behind "cluster in shade" / "only spawn this
 * species in a sunlit gap".
 *
 * SIGN CONVENTION — the single most likely misuse. The result lies in (0, 1]:
 *     1  = FULL SUN (nothing occludes the point)
 *    ->0 = DEEP SHADE
 * It is transmitted light, NOT an occlusion amount. Filtering for shade means filtering for a value
 * BELOW a threshold.
 *
 * MATHS, per target point:
 *     Sum           = SUM over occluders j reaching the point of  Density_j * Falloff(d, R_j)
 *     Transmittance = Exp(-ExtinctionK * Sum)
 * i.e. Beer-Lambert with Sum standing in for optical depth. Each occluder carries its OWN radius R_j and
 * its own density (the LAI analogue), so a big tree shades further and harder than a shrub. The falloff
 * kernel is EArcPCGFalloff from Scatter/ArcPCGScatterCore.h, the sole authority for that maths; the hard
 * cutoff there is what makes R_j a true reach.
 *
 * NO ENGINE EQUIVALENT (checked against the whole PCG Elements inventory): UPCGBlurSettings
 * (PCGBlurElement.h:30) is the closest machinery but computes a NORMALIZED weighted mean — its weights
 * sum to 1 — which is exactly the wrong semantics for accumulated occlusion; it also has no per-element
 * radius and only one input pin. UPCGDistanceSettings is nearest-distance only.
 *
 * NOT FArcForestLightModel::ComputeIncidentLight: that model is height-ordered and band-aware by
 * construction and states its transmittance raster cannot be recovered after the sweep
 * (ArcForestLightModel.h:90-101). A standalone node has neither height ordering nor bands, so a plain
 * octree sum is both simpler and exact.
 *
 * AN UNCONNECTED OCCLUDERS PIN MEANS AN EMPTY OCCLUDER SET, so Sum is 0 and every target gets EXACTLY
 * 1.0. It deliberately does NOT mean self-occlusion: that would hand every target Exp(-k*Density) with no
 * way left to express "no shade", and would make the 1.0 identity unobservable. A user who wants
 * self-shading wires the same points into BOTH pins, which the self-exclusion rule below then governs.
 *
 * EXPLICIT OPT-IN FOR THE PER-OCCLUDER ATTRIBUTES. bUseOccluderRadiusAttribute and
 * bUseOccluderDensityAttribute default to FALSE, and the selectors are hidden behind them. This is not
 * cosmetic: FPCGAttributePropertyInputSelector default-CONSTRUCTS to @Last
 * (PCGAttributePropertySelector.cpp:511-514), so an untouched selector resolves to whatever the occluder
 * data's last attribute happens to be — a PatchId from Clustered Scatter, say — which, being numeric, is
 * silently accepted as a radius or a density. The constant would then be reached only when nothing
 * resolved at all. With the toggles off NO accessor is created and the constant is used unconditionally,
 * so the defaults mean "use the constants", which is the safe reading. The resolve-to-nothing fallback
 * (constant + one graph warning) still applies when a toggle IS on.
 *
 * SELF-EXCLUSION when the same points are on both pins: a contribution is skipped only when the occluder
 * data POINTER and the point INDEX both match the target, and bIncludeSelf is off. Pointer identity is
 * required — an index-only test would wrongly drop the cross-set pair (occluder 7, target 7) whenever two
 * genuinely different sets are wired up.
 *
 * ONE INPUT DATA AT A TIME on the Points pin, one output data per input data. Targets never leak across
 * a data boundary. Occluders are the exception by design: EVERY occluder data shades EVERY target data,
 * because that is what "this canopy shades that undergrowth" means on a graph.
 *
 * THREADING: no surface sampling, so CanExecuteOnlyOnMainThread and IsCacheable stay at their defaults.
 * The occluder loop is nonetheless SERIAL — it scatter-writes into a per-target accumulator, which a bare
 * ParallelFor would race on; parallelising it needs per-thread accumulators, not a flag flip.
 *
 * DENSITY *AND* ATTRIBUTE, both, always: the transmittance is written into the output Density under
 * DensityMode and into the named float attribute. Density is where PCG expects a per-point scalar and is
 * the ONLY thing the debug display shades by (PCGSpatialDataVisualization.cpp:245), while the attribute is
 * what downstream filters/spawners select by name. UPCGPointNeighborhoodSettings writes both for the same
 * reason. Transmittance already lies in (0, 1], so no normalisation is needed before Set.
 *
 * PASS-THROUGH: points, attributes and tags are copied by ArcPCGScatterCore::PassThroughPoints, the sole
 * authority for that sequence. Only the density write and the transmittance attribute are added.
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), meta = (Keywords = "arc extension shade transmittance occlusion light"))
class UPCGShadeAttributeSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("ShadeAttribute")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGShadeAttribute", "NodeTitle", "Arc Shade Attribute"); }
	virtual FText GetNodeTooltipText() const override;
	// Metadata: the point set is passed through unchanged and only decorated with an attribute derived
	// from a second data set.
	/** PointOps for all five Arc nodes — see the note on UPCGScatterNearPointsSettings::GetType. */
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::PointOps; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }
	virtual FPCGElementPtr CreateElement() const override;

public:
	/** Beer-Lambert extinction coefficient. Transmittance = Exp(-ExtinctionK * Sum), so doubling this
	 *  SQUARES the transmittance. Zero means the canopy is perfectly transparent (everything reads 1). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Shade", meta = (ClampMin = "0.0", PCG_Overridable))
	float ExtinctionK = 0.5f;

	/** Read the shade reach per occluder from an attribute instead of using Occluder Radius Constant Cm.
	 *  OFF by default, and that default is load-bearing — see the EXPLICIT OPT-IN block in the class
	 *  comment. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Shade", meta = (PCG_Overridable))
	bool bUseOccluderRadiusAttribute = false;

	/** Per-occluder shade reach in cm, read off the occluder points. Only consulted when Use Occluder Radius
	 *  Attribute is on; falls back to Occluder Radius Constant Cm — with one graph warning — when it resolves
	 *  to nothing or to a non-numeric attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Shade", meta = (PCG_Overridable, EditCondition = "bUseOccluderRadiusAttribute", EditConditionHides))
	FPCGAttributePropertyInputSelector OccluderRadiusAttribute;

	/** Shade reach used for every occluder when Use Occluder Radius Attribute is off, or when the selector
	 *  does not resolve. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Shade", meta = (ClampMin = "0.0", PCG_Overridable))
	double OccluderRadiusConstantCm = 300.0;

	/** Read the optical density per occluder from an attribute instead of using Occluder Density Constant.
	 *  OFF by default, same reasoning as the radius toggle. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Shade", meta = (PCG_Overridable))
	bool bUseOccluderDensityAttribute = false;

	/** Per-occluder optical density (the LAI analogue) read off the occluder points: how hard one occluder
	 *  shades at its own centre. Only consulted when Use Occluder Density Attribute is on; same fallback as
	 *  the radius selector. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Shade", meta = (PCG_Overridable, EditCondition = "bUseOccluderDensityAttribute", EditConditionHides))
	FPCGAttributePropertyInputSelector OccluderDensityAttribute;

	/** Optical density used for every occluder when Use Occluder Density Attribute is off, or when the
	 *  selector does not resolve. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Shade", meta = (ClampMin = "0.0", PCG_Overridable))
	float OccluderDensityConstant = 1.0f;

	/** How an occluder's contribution decays from its centre out to its own radius. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Shade", meta = (PCG_Overridable))
	EArcPCGFalloff Falloff = EArcPCGFalloff::Linear;

	/** Let a point shade itself when the same points are wired into both pins. Because the kernel is exactly
	 *  1 at distance 0, enabling this multiplies every transmittance by exactly Exp(-K * Density). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Shade", meta = (PCG_Overridable))
	bool bIncludeSelf = false;

	/** How the transmittance is combined with each point's existing Density. Density is where PCG expects a
	 *  per-point scalar, and it is what the debug display shades by; the attribute below is written too, the
	 *  two coexist. Transmittance is already in (0, 1], so Set needs no normalisation step. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Shade", meta = (PCG_Overridable))
	EArcPCGDensityMode DensityMode = EArcPCGDensityMode::Set;

	/** Name of the float attribute written onto the output points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Shade", meta = (PCG_Overridable))
	FName OutputAttributeName = TEXT("Transmittance");
};

class FPCGShadeAttributeElement : public IPCGElement
{
protected:
	virtual bool SupportsBasePointDataInputs(FPCGContext* InContext) const override { return true; }
	// Single-pass: returns true on the first call, always. No custom context, no PauseUntilNextFrame, no
	// worker task, no AbortInternal. CanExecuteOnlyOnMainThread and IsCacheable stay at their defaults —
	// there is no surface sampling here, so neither the GT pin nor the landscape-CRC gap applies.
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
