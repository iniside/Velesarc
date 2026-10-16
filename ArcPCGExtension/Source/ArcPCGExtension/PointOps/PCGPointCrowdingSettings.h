// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "PCGSettings.h"
#include "PCGElement.h"

#include "Scatter/ArcPCGScatterCore.h"

#include "PCGPointCrowdingSettings.generated.h"

namespace PCGPointCrowdingConstants
{
	// `extern` + a .cpp definition, NOT a header-scope `const FName`: the latter gives every including
	// translation unit its own copy plus a load-time FName constructor. This is the engine idiom
	// (PCGCommon.h:389-397 / PCGCommon.cpp:556-558), and the form both scatter siblings use.

	/** Points whose local neighbourhood is measured. Also the points that are emitted. */
	extern ARCPCGEXTENSION_API const FName PointsLabel;
}

/**
 * Writes a per-point CROWDING score — the falloff-weighted count of the point's neighbours inside
 * SearchRadiusCm — onto the input points, which are otherwise passed through unchanged. The graph-level
 * primitive behind "thin out where it is dense" and "shrink meshes in crowded clumps".
 *
 * NO ENGINE EQUIVALENT (checked against the whole PCG Elements inventory): UPCGPointNeighborhoodSettings
 * does NOT count neighbours — it delegates to ProjectPoint for one averaged neighbour
 * (PCGPointNeighborhood.cpp:161) and discards the count. UPCGDistanceSettings is nearest-distance only.
 * The UPCGBlurSettings workaround fails too: blurring a constant 1.0 returns 1.0, not N, because Blur
 * computes a NORMALIZED weighted mean (weights sum to 1), which is the wrong semantics for an
 * accumulated count.
 *
 * WEIGHTING: the kernel is EArcPCGFalloff from Scatter/ArcPCGScatterCore.h, the sole authority for the
 * maths. Constant mode therefore yields the EXACT INTEGER neighbour count inside the radius (the kernel
 * guarantees exactly 1 at d == 0 and exactly 0 past R); Linear and Gaussian yield a smooth density
 * proxy instead. Nothing here re-derives the kernel.
 *
 * ONE INPUT DATA AT A TIME, one output data per input data: a neighbour query NEVER crosses a data
 * boundary, because each tagged data is its own point set. Wiring two separate point sets in and
 * expecting them to shade each other is the likely misuse — union them upstream (Merge/Gather) if
 * cross-set crowding is what is wanted.
 *
 * THREADING: this node samples NO surface — it only reads an octree it does not mutate — so
 * CanExecuteOnlyOnMainThread and IsCacheable are both left at their defaults, unlike the scatter
 * siblings which are pinned to the game thread by UPCGSpatialData::ProjectPoint.
 *
 * DENSITY *AND* ATTRIBUTE, both, always: the score is written into the output Density under DensityMode
 * and into the named float attribute. That is not a duplicate — Density is where PCG expects a per-point
 * scalar and is the ONLY thing the debug display shades by (PCGSpatialDataVisualization.cpp:245), while
 * the attribute is what downstream filters/spawners select by name. UPCGPointNeighborhoodSettings writes
 * both for the same reason. Because the raw score is an unbounded count and Density is 0-1, bNormalize
 * defaults to TRUE here.
 *
 * PASS-THROUGH: points, attributes and tags are copied by ArcPCGScatterCore::PassThroughPoints, which is
 * the sole authority for that sequence. Only the density write and the crowding attribute are added.
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), meta = (Keywords = "arc extension crowding neighbour density"))
class UPCGPointCrowdingSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("PointCrowding")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGPointCrowding", "NodeTitle", "Arc Point Crowding"); }
	virtual FText GetNodeTooltipText() const override;
	// PointOps: the point set is passed through unchanged and only decorated with an attribute.
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::PointOps; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }
	virtual FPCGElementPtr CreateElement() const override;

public:
	/** Neighbourhood radius in cm. It is a TRUE support radius: a neighbour beyond it contributes exactly
	 *  zero in every falloff mode, not a small residual. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Crowding", meta = (ClampMin = "0.0", PCG_Overridable))
	double SearchRadiusCm = 500.0;

	/** How a neighbour's contribution decays with distance. Constant gives the exact integer neighbour
	 *  count; Linear and Gaussian give a smooth density proxy. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Crowding", meta = (PCG_Overridable))
	EArcPCGFalloff Falloff = EArcPCGFalloff::Linear;

	/** Count the point itself as one of its own neighbours. Because the kernel is exactly 1 at distance 0,
	 *  enabling this shifts EVERY value by exactly 1 before normalisation. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Crowding", meta = (PCG_Overridable))
	bool bIncludeSelf = false;

	/** Rescale so the busiest point in EACH input data reads exactly 1. The maximum is per input data, not
	 *  global, so two input datas normalise independently. Left alone when every value is ~0.
	 *
	 *  ON BY DEFAULT because the raw score is an UNBOUNDED weighted neighbour count while Density is 0-1:
	 *  with it off, Set mode clamps almost every point to 1 and the output looks uniformly dense. Turn it
	 *  off only when you want the raw count in the attribute and are handling the density range yourself. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Crowding", meta = (PCG_Overridable))
	bool bNormalize = true;

	/** How the crowding score is combined with each point's existing Density. Density is where PCG expects
	 *  a per-point scalar, and it is what the debug display shades by; the attribute below is written too,
	 *  the two coexist. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Crowding", meta = (PCG_Overridable))
	EArcPCGDensityMode DensityMode = EArcPCGDensityMode::Set;

	/** Name of the float attribute written onto the output points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Crowding", meta = (PCG_Overridable))
	FName OutputAttributeName = TEXT("Crowding");
};

class FPCGPointCrowdingElement : public IPCGElement
{
protected:
	virtual bool SupportsBasePointDataInputs(FPCGContext* InContext) const override { return true; }
	// Single-pass: returns true on the first call, always. No custom context, no PauseUntilNextFrame, no
	// worker task, no AbortInternal. CanExecuteOnlyOnMainThread and IsCacheable stay at their defaults —
	// there is no surface sampling here, so neither the GT pin nor the landscape-CRC gap applies.
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
