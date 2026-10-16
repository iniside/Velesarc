// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Math/RandomStream.h"
#include "Templates/Function.h"

#include "ArcPCGScatterCore.generated.h"

class UPCGBasePointData;
class UPCGSpatialData;
struct FPCGContext;
struct FPCGPoint;
struct FPCGTaggedData;

/**
 * Shared, stateless primitives behind the standalone ArcPCGExtension graph nodes (Scatter Near
 * Points, Clustered Scatter, Point Crowding, Shade Attribute, Radial Thinning).
 *
 * WHY THIS FILE EXISTS: five separately-authored nodes otherwise each re-derive annulus sampling,
 * the falloff kernel, the seeding mix and — most dangerously — the point pass-through, and then
 * diverge on exactly the metadata/tag details that are easiest to get subtly wrong. This is the
 * single authority for all of them.
 *
 * PURITY: UObject-free except for the two PCG-facing helpers (ProjectOntoSurface,
 * PassThroughPoints). The sampling/falloff/seeding functions are pure math and allocation-free.
 *
 * PRECISION: all geometry is double / FVector2D / FVector, matching PCG transforms. Never round-trip
 * through float / FVector2f — the forest sim does, and its positions are world-space centimetres
 * where float loses metres of precision on a large landscape.
 *
 * ------------------------------------------------------------------------------------------------
 * HOUSE SHAPE — the form every node built on this core follows (from the local precedent
 * PCGForestSimSettings.{h,cpp}). Stated once, here, so the five nodes stay consistent:
 *
 *   - UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural)) on the settings class.
 *   - Editor-only overrides (GetDefaultNodeName, GetDefaultNodeTitle, GetNodeTooltipText) live
 *     inside #if WITH_EDITOR.
 *   - The .cpp starts with #include UE_INLINE_GENERATED_CPP_BY_NAME(<Name>) and carries a
 *     #define LOCTEXT_NAMESPACE "..." / #undef LOCTEXT_NAMESPACE pair.
 *   - .generated.h is LAST in the header's include list.
 *   - IPCGElement::SupportsBasePointDataInputs() -> true on EVERY element. The default false makes
 *     PCG run a costly input conversion (PCGElement.h:248).
 *   - No `auto`, anywhere. Every type is spelled out, including range-for and lambda returns.
 * ------------------------------------------------------------------------------------------------
 */

/**
 * Distance-weighting kernel shared by the crowding, shade and thinning nodes.
 *
 * A UENUM because Steps 5/6 expose it directly as a UPROPERTY on their settings classes.
 */
UENUM(BlueprintType)
enum class EArcPCGFalloff : uint8
{
	/** Every neighbour inside the radius contributes 1. Outside the radius, 0. */
	Constant,
	/** Linear ramp: 1 at the centre, 0 at the radius. */
	Linear,
	/** Gaussian with sigma = Radius/3, so the radius is the ~3-sigma cutoff. */
	Gaussian
};

/**
 * How a node's computed per-point scalar is combined with the point's EXISTING Density.
 *
 * PCG convention: a per-point scalar belongs on Density, with a mode. This mirrors
 * PCGNormalToDensityMode (Elements/PCGNormalToDensity.h:9-19) verbatim in its member set — the same
 * seven ops, in the same order — rather than the shorter EPCGAttributeNoiseMode
 * (Elements/PCGAttributeNoise.h:16-24), which has no Subtract and no Divide.
 *
 * Unlike the engine enum this one is BlueprintType/uint8, because the Arc nodes expose it as a
 * PCG_Overridable UPROPERTY that Blueprint and the overrides pin have to see.
 */
UENUM(BlueprintType)
enum class EArcPCGDensityMode : uint8
{
	/** Replace the existing density with the computed value. */
	Set,
	/** Keep the smaller of the two. */
	Minimum,
	/** Keep the larger of the two. */
	Maximum,
	/** Existing + value. */
	Add,
	/** Existing - value. */
	Subtract,
	/** Existing * value. */
	Multiply,
	/** Existing / value. A zero divisor leaves the existing density untouched. */
	Divide
};

namespace ArcPCGScatterCore
{
	/**
	 * Per-node salts, distinct so two nodes sharing one Seed value decorrelate instead of replaying
	 * the same dart sequence. Never reuse a value; never derive one from another.
	 */
	constexpr uint32 ScatterNearPointsSalt = 0x5CA77E01u;
	constexpr uint32 ClusteredScatterParentSalt = 0x5CA77E02u;
	constexpr uint32 ClusteredScatterOffspringSalt = 0x5CA77E03u;
	constexpr uint32 PointCrowdingSalt = 0x5CA77E04u;
	constexpr uint32 ShadeAttributeSalt = 0x5CA77E05u;
	constexpr uint32 RadialThinningSalt = 0x5CA77E06u;

	/**
	 * Area-uniform point in the annulus [MinRadiusCm, MaxRadiusCm], as an XY offset from the centre.
	 *
	 *   r = Sqrt(Lerp(Min^2, Max^2, U)),  theta = 2*PI*V
	 *
	 * The square-root is what makes it AREA-uniform: a naive Lerp(Min, Max, U) over-weights the
	 * inner ring, visibly bunching scatter against its source point.
	 *
	 * DETERMINISM CONTRACT: exactly TWO draws, UNCONDITIONAL, in the order (U, V). Callers rely on
	 * the stream staying aligned across points regardless of what they do with the result, so no
	 * early-out may be added in front of the draws.
	 *
	 * Negative radii are clamped to 0 before squaring. An inverted range (Min > Max) is NOT fixed
	 * here — squaring makes it behave as the range [Max, Min]; the calling node is responsible for
	 * clamping and warning (see Step 2).
	 */
	ARCPCGEXTENSION_API FVector2D SampleAnnulus(FRandomStream& Rng, double MinRadiusCm, double MaxRadiusCm);

	/**
	 * Isotropic 2D Gaussian offset with standard deviation SigmaCm, via Box-Muller.
	 *
	 * Lifted verbatim in form from the forest sim's offspring block (ArcForestSimulator.cpp:310-317):
	 *   Radius = Sigma * Sqrt(-2 * Loge(Max(U1, UE_SMALL_NUMBER)));  Theta = 2*PI*U2
	 *
	 * FIXED-COUNT, no rejection: U1 is guarded off zero for the log rather than redrawn, U2 is the
	 * angle and needs no guard. Exactly TWO draws, UNCONDITIONAL, in the order (U1, U2).
	 */
	ARCPCGEXTENSION_API FVector2D SampleGaussianOffset(FRandomStream& Rng, double SigmaCm);

	/**
	 * Distance weight in [0, 1].
	 *
	 *   Constant -> 1
	 *   Linear   -> Max(0, 1 - d/R)
	 *   Gaussian -> Exp(-0.5 * (d / (R/3))^2)
	 *
	 * GUARANTEES, in every mode, that consumers and tests depend on:
	 *   - returns EXACTLY 0 for d > R (hard cutoff, so a search radius is a true support radius),
	 *   - returns EXACTLY 1 at d == 0 (so Constant mode over a neighbourhood yields the integer
	 *     neighbour count, and bIncludeSelf shifts every value by a known amount).
	 * R == 0 therefore degenerates to a Dirac at d == 0, with no division by zero. A NEGATIVE R is
	 * caught by the d > R cutoff and returns 0 even at d == 0 — the centre guarantee holds for R >= 0.
	 *
	 * Note the cutoff is STRICT (d > R). Gaussian at exactly d == R is Exp(-4.5) ~= 0.011, not 0;
	 * that ~0.011 step at the support edge is deliberate, not a rounding artefact.
	 */
	ARCPCGEXTENSION_API float EvaluateFalloff(EArcPCGFalloff Mode, double Distance, double Radius);

	/**
	 * Combines a node's computed scalar with a point's existing Density under Mode, and CLAMPS the
	 * result to [0, 1] — PCG density is normalised, and the debug display shades by it
	 * (PCGSpatialDataVisualization.cpp:245), so an out-of-range value is not merely odd but unshown.
	 *
	 * ExistingDensity is the LEFT operand throughout: Subtract is Existing - Value, Divide is
	 * Existing / Value. Divide by a zero divisor is NOT an error and NOT a clamp to 1 — it returns
	 * ExistingDensity untouched, so a node whose scalar is legitimately 0 somewhere leaves those
	 * points alone instead of blowing them out.
	 *
	 * Set ignores ExistingDensity entirely, which is what makes it the safe default: it needs no
	 * assumption about whether the input's Density was ever allocated.
	 */
	ARCPCGEXTENSION_API float ApplyDensityMode(EArcPCGDensityMode Mode, float ExistingDensity, float Value);

	/**
	 * Drops a vertical query at (XY, QueryZ) onto Surface. Returns false on a miss (off-surface, or
	 * a cold GT-only cache) — callers must DROP the sample, never substitute a fallback Z; a
	 * floating point is worse than a missing one.
	 *
	 * Mirrors PCGForestSimSettings.cpp:252 exactly: default-constructed FPCGProjectionParams,
	 * FBox(ForceInit), nullptr out-metadata. On success OutPoint.Density carries surface suitability
	 * and OutPoint.Transform.GetLocation().Z the surface height.
	 *
	 * MAIN-THREAD: UPCGSpatialData::ProjectPoint can trigger GT-only cache creation on landscape, so
	 * every element calling this must return true from CanExecuteOnlyOnMainThread
	 * (rationale at PCGForestSimSettings.h:26-35).
	 */
	ARCPCGEXTENSION_API bool ProjectOntoSurface(const UPCGSpatialData* Surface, const FVector2D& XY, double QueryZ,
		FPCGPoint& OutPoint);

	/**
	 * Chunk-invariant per-point stream: mixes the node Seed, a per-node salt, and a POSITION-derived
	 * seed (PCGHelpers::ComputeSeedFromPosition).
	 *
	 * POSITION, never a loop index. Point indices restart at 0 in every tagged data, so an
	 * index-derived stream replays identical offsets when the same input arrives split across two
	 * data — the node's output would then depend on upstream chunking. This helper is the reason it
	 * does not.
	 */
	ARCPCGEXTENSION_API FRandomStream MakeStreamForPosition(int32 NodeSeed, uint32 Salt, const FVector& Position);

	/**
	 * Deterministic index-derived stream, for GENERATORS with no input points (node D only) where
	 * there is no position to key on and the sequence is inherently serial.
	 *
	 * Shape from MakeRecruitStream (ArcForestSimulator.cpp:788-794): salt XOR plus an odd
	 * golden-ratio multiply to decorrelate consecutive indices, masked to a non-negative seed so the
	 * mix never depends on signed-overflow behaviour inside FRandomStream.
	 */
	ARCPCGEXTENSION_API FRandomStream MakeStreamForIndex(int32 NodeSeed, uint32 Salt, int32 Index);

	/**
	 * Shared pass-through for the point-op nodes (crowding, shade, thinning): copies the subset
	 * `Indices` of InPointData into a fresh point data and appends it to Context->OutputData,
	 * preserving tags, attributes and metadata parentage. Returns the new point data so the caller
	 * can add its own attribute, or nullptr on bad input.
	 *
	 * PRECONDITION: every entry of `Indices` must lie in [0, InPointData->GetNumPoints()).
	 * SetPointsFrom indexes the source ranges unchecked (PCGBasePointData.cpp:792-801). An EMPTY
	 * Indices is fine and yields an empty output data.
	 *
	 * The four steps below are load-bearing and ordered:
	 *   1. FPCGContext::NewPointData_AnyThread — NEVER NewObject<UPCGPointData>.
	 *   2. InitializeFromDataWithParams — NOT InitializeFromData (declared PCGSpatialData.h:247;
	 *      the comment at :242 recommends the WithParams form). This parents the output metadata,
	 *      which is what makes the MetadataEntry keys copied in step 3 resolve. bInheritSpatialData
	 *      is turned OFF first: we write a subset, so inheriting per-index property arrays makes
	 *      step 3 flatten all N source points before truncating.
	 *   3. SetPointsFrom(In, Indices) (PCGBasePointData.h:282).
	 *   4. Context->OutputData.TaggedData.Add_GetRef(InTagged), then overwrite .Data. Tags live on
	 *      FPCGTaggedData, NOT on UPCGData — neither Initialize* nor SetPointsFrom copies them, so
	 *      dropping this step silently drops every input tag.
	 *
	 * PIN LABEL: the copied FPCGTaggedData inherits the INPUT pin's label. That is correct for these
	 * nodes only because each has exactly one output pin, and PCGElement.cpp:1209-1218 rewrites the
	 * label of every non-pinless output when a settings object declares a single output pin — gated
	 * on !IsPassthrough(Settings) at :1204, which none of these nodes override off its false default.
	 * A node with two or more output pins must overwrite OutTagged.Pin itself.
	 *
	 * Copying the WHOLE input struct (rather than only Tags) is also required, not merely tidy:
	 * the per-data caching path filters outputs by TaggedData.OriginalIndex (PCGElement.cpp:866).
	 */
	ARCPCGEXTENSION_API UPCGBasePointData* PassThroughPoints(FPCGContext* Context, const FPCGTaggedData& InTagged,
		const UPCGBasePointData* InPointData, TArrayView<const int32> Indices);

	/**
	 * Calls InCallback for every point of InPointData whose location lies within InRadiusCm of InCenter,
	 * passing the point INDEX and its SQUARED distance to the centre. Read-only: the octree is primed and
	 * traversed, never mutated.
	 *
	 * WHY THIS EXISTS instead of UPCGOctreeQueries::ForEachPointInsideSphere
	 * (SpatialAlgo/PCGOctreeQueries.h:24-25): that helper is a TEMPLATE declared PCG_API and DEFINED in the
	 * same header, so outside the PCG module PCG_API expands to dllimport and the header's own definition
	 * is ill-formed — MSVC C2491, "definition of dllimport function not allowed". It is therefore usable
	 * only inside the PCG module, and no amount of include ordering fixes that. This is the SAME query,
	 * implemented against the same public octree (PCGBasePointData.h:275 GetPointOctree, :108 IsValidRef),
	 * deliberately keeping the engine's exact behaviour including the Sqrt(2)-scaled search extent, which
	 * over-fetches and is then filtered by the exact squared-distance test.
	 *
	 * The callback is a TFunctionRef rather than a template parameter so this stays a normal exported
	 * function; the indirect call is immaterial next to the octree traversal it sits inside.
	 *
	 * PRIMING: the caller should call InPointData->PrimePointOctree() once before a batch of queries,
	 * rather than letting the first query build it lazily.
	 */
	ARCPCGEXTENSION_API void ForEachPointInsideSphere(const UPCGBasePointData* InPointData, const FVector& InCenter,
		double InRadiusCm, TFunctionRef<void(int32 PointIndex, double SquaredDistance)> InCallback);
}
