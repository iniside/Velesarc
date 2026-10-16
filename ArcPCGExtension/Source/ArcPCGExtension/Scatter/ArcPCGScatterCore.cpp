// Copyright Lukasz Baran. All Rights Reserved.

#include "Scatter/ArcPCGScatterCore.h"

#include "PCGContext.h"
#include "PCGData.h"
#include "PCGPoint.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGSpatialData.h"
#include "Elements/PCGProjectionParams.h"
#include "Helpers/PCGHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcPCGScatterCore)

namespace ArcPCGScatterCorePrivate
{
	/**
	 * Knuth's odd golden-ratio multiplier, the same constant the forest sim's MakeRecruitStream uses
	 * (ArcForestSimulator.cpp:788-794). Odd, so the multiply is a bijection on uint32 and cannot
	 * collapse distinct inputs onto one seed.
	 */
	constexpr uint32 GoldenRatioMultiplier = 2654435761u;

	/** Shared tail of both stream builders: mask to a non-negative seed, never rely on signed overflow. */
	FRandomStream MakeStreamFromMix(uint32 Mixed)
	{
		return FRandomStream(static_cast<int32>(Mixed & 0x7FFFFFFFu));
	}
}

namespace ArcPCGScatterCore
{
	FVector2D SampleAnnulus(FRandomStream& Rng, double MinRadiusCm, double MaxRadiusCm)
	{
		// THE FIXED 2-DRAW BLOCK (determinism-critical): both draws are UNCONDITIONAL and precede
		// every use below, so the stream stays aligned no matter what the radii are.
		const double U = static_cast<double>(Rng.FRand());
		const double V = static_cast<double>(Rng.FRand());

		const double MinR = FMath::Max(MinRadiusCm, 0.0);
		const double MaxR = FMath::Max(MaxRadiusCm, 0.0);

		// Area-uniform: interpolate the SQUARED radii, then take the root. FMath::Max guards only
		// against a negative from rounding — both endpoints are already non-negative.
		const double RadiusSquared = FMath::Max(FMath::Lerp(MinR * MinR, MaxR * MaxR, U), 0.0);
		const double Radius = FMath::Sqrt(RadiusSquared);
		const double Theta = 2.0 * UE_DOUBLE_PI * V;

		return FVector2D(Radius * FMath::Cos(Theta), Radius * FMath::Sin(Theta));
	}

	FVector2D SampleGaussianOffset(FRandomStream& Rng, double SigmaCm)
	{
		// THE FIXED 2-DRAW BLOCK (determinism-critical), same contract as SampleAnnulus.
		const double U1 = static_cast<double>(Rng.FRand());
		const double U2 = static_cast<double>(Rng.FRand());

		// Box-Muller, FIXED-COUNT with no rejection: U1 is guarded off zero for Loge (a redraw would
		// desynchronise the stream), U2 is the angle and needs no guard.
		const double GuardedU1 = FMath::Max(U1, static_cast<double>(UE_SMALL_NUMBER));
		const double Radius = SigmaCm * FMath::Sqrt(-2.0 * FMath::Loge(GuardedU1));
		const double Theta = 2.0 * UE_DOUBLE_PI * U2;

		return FVector2D(Radius * FMath::Cos(Theta), Radius * FMath::Sin(Theta));
	}

	float EvaluateFalloff(EArcPCGFalloff Mode, double Distance, double Radius)
	{
		// Hard cutoff FIRST: a search radius is a true support radius in every mode, and this is
		// what lets the divisions below assume Radius >= Distance > 0.
		if (Distance > Radius)
		{
			return 0.0f;
		}

		// Exactly 1 at the centre in every mode. Also the Radius == 0 degenerate case, which reduces
		// to a Dirac at d == 0 rather than dividing by zero. (A NEGATIVE radius is caught by the
		// cutoff above and returns 0 even at d == 0 — the centre guarantee is stated for R >= 0.)
		if (Distance <= 0.0)
		{
			return 1.0f;
		}

		switch (Mode)
		{
		case EArcPCGFalloff::Constant:
			return 1.0f;

		case EArcPCGFalloff::Linear:
			return static_cast<float>(FMath::Max(0.0, 1.0 - (Distance / Radius)));

		case EArcPCGFalloff::Gaussian:
		{
			// Sigma = Radius/3, so the radius is the ~3-sigma cutoff and the discontinuity the hard
			// cutoff above introduces at d == R is ~0.011, not a visible step.
			const double Sigma = Radius / 3.0;
			const double Normalised = Distance / Sigma;
			return static_cast<float>(FMath::Exp(-0.5 * Normalised * Normalised));
		}

		default:
			return 0.0f;
		}
	}

	float ApplyDensityMode(EArcPCGDensityMode Mode, float ExistingDensity, float Value)
	{
		switch (Mode)
		{
		case EArcPCGDensityMode::Set:
			return FMath::Clamp(Value, 0.0f, 1.0f);

		case EArcPCGDensityMode::Minimum:
			return FMath::Clamp(FMath::Min(ExistingDensity, Value), 0.0f, 1.0f);

		case EArcPCGDensityMode::Maximum:
			return FMath::Clamp(FMath::Max(ExistingDensity, Value), 0.0f, 1.0f);

		case EArcPCGDensityMode::Add:
			return FMath::Clamp(ExistingDensity + Value, 0.0f, 1.0f);

		case EArcPCGDensityMode::Subtract:
			return FMath::Clamp(ExistingDensity - Value, 0.0f, 1.0f);

		case EArcPCGDensityMode::Multiply:
			return FMath::Clamp(ExistingDensity * Value, 0.0f, 1.0f);

		case EArcPCGDensityMode::Divide:
			// Zero divisor: leave the point ALONE, unclamped and unmodified. Not 0, not 1 — a scalar
			// that happens to be 0 at some points (an unshaded gap, an isolated point with no
			// neighbours) must not blow those points out.
			if (Value == 0.0f)
			{
				return ExistingDensity;
			}
			return FMath::Clamp(ExistingDensity / Value, 0.0f, 1.0f);

		default:
			return FMath::Clamp(ExistingDensity, 0.0f, 1.0f);
		}
	}

	bool ProjectOntoSurface(const UPCGSpatialData* Surface, const FVector2D& XY, double QueryZ,
		FPCGPoint& OutPoint)
	{
		if (!Surface)
		{
			return false;
		}

		// Mirrors PCGForestSimSettings.cpp:252 exactly.
		const FPCGProjectionParams ProjParams;
		const FTransform QueryTransform(FVector(XY.X, XY.Y, QueryZ));

		return Surface->ProjectPoint(QueryTransform, FBox(ForceInit), ProjParams, OutPoint, nullptr);
	}

	FRandomStream MakeStreamForPosition(int32 NodeSeed, uint32 Salt, const FVector& Position)
	{
		const uint32 PositionSeed = static_cast<uint32>(PCGHelpers::ComputeSeedFromPosition(Position));
		const uint32 Mixed = static_cast<uint32>(NodeSeed)
			^ Salt
			^ (PositionSeed * ArcPCGScatterCorePrivate::GoldenRatioMultiplier);

		return ArcPCGScatterCorePrivate::MakeStreamFromMix(Mixed);
	}

	FRandomStream MakeStreamForIndex(int32 NodeSeed, uint32 Salt, int32 Index)
	{
		const uint32 Mixed = static_cast<uint32>(NodeSeed)
			^ Salt
			^ (static_cast<uint32>(Index) * ArcPCGScatterCorePrivate::GoldenRatioMultiplier);

		return ArcPCGScatterCorePrivate::MakeStreamFromMix(Mixed);
	}

	UPCGBasePointData* PassThroughPoints(FPCGContext* Context, const FPCGTaggedData& InTagged,
		const UPCGBasePointData* InPointData, TArrayView<const int32> Indices)
	{
		if (!Context || !InPointData)
		{
			return nullptr;
		}

		// 1. Allocation goes through the context so PCG picks the point-data class its CVar selects.
		UPCGBasePointData* OutPointData = FPCGContext::NewPointData_AnyThread(Context);
		if (!OutPointData)
		{
			return nullptr;
		}

		// 2. The WithParams form (declared PCGSpatialData.h:247) — this is what parents the output
		//    metadata, without which the MetadataEntry keys copied in step 3 resolve to nothing.
		//    bInheritSpatialData MUST be off: we write a SUBSET, and inheriting the parent's
		//    per-index property arrays makes step 3's SetNumPoints flatten all N source points
		//    before truncating to Indices.Num(). Same reason, same fix as the three engine sites
		//    that do this exact sequence — PCGCullPointsOutsideActorBounds.cpp:173,
		//    PCGSpawnActor.cpp:89, PCGSelfPruning.cpp:697. bInheritMetadata/bInheritAttributes
		//    stay on; those are what make step 3's copied entry keys resolve.
		FPCGInitializeFromDataParams InitParams(InPointData);
		InitParams.bInheritSpatialData = false;
		OutPointData->InitializeFromDataWithParams(InitParams);

		// 3. Copy the selected subset of points, properties and metadata entries.
		OutPointData->SetPointsFrom(InPointData, Indices);

		// 4. Tags live on FPCGTaggedData, NOT on UPCGData: neither Initialize* nor SetPointsFrom
		//    carries them, so the output entry is COPIED from the input and only .Data overwritten.
		//    Dropping this would silently drop every input tag.
		FPCGTaggedData& OutTagged = Context->OutputData.TaggedData.Add_GetRef(InTagged);
		OutTagged.Data = OutPointData;

		return OutPointData;
	}

	void ForEachPointInsideSphere(const UPCGBasePointData* InPointData, const FVector& InCenter,
		double InRadiusCm, TFunctionRef<void(int32 PointIndex, double SquaredDistance)> InCallback)
	{
		if (!InPointData)
		{
			return;
		}

		// Mirrors UPCGOctreeQueries::ForEachPointInsideSphere (PCGOctreeQueries.h:71-109) exactly — see the
		// header for why that template cannot be called from outside the PCG module. The Sqrt(2)-scaled
		// extent is the engine's, kept verbatim: it over-fetches the box query, and the exact
		// squared-distance test below is what makes the result a true sphere.
		const double Extents = UE_DOUBLE_SQRT_2 * InRadiusCm;
		const double SquaredRadius = InRadiusCm * InRadiusCm;
		const FBoxCenterAndExtent SearchBounds(InCenter, FVector(Extents, Extents, Extents));

		const FPCGPointTransform::ConstValueRange TransformValueRange = InPointData->GetConstTransformValueRange();

		InPointData->GetPointOctree().FindElementsWithBoundsTest(SearchBounds,
			[InPointData, &TransformValueRange, &InCenter, SquaredRadius, &InCallback](const PCGPointOctree::FPointRef& PointRef) -> void
			{
				if (!InPointData->IsValidRef(PointRef))
				{
					return;
				}

				const double SquaredDistance =
					FVector::DistSquared(InCenter, TransformValueRange[PointRef.Index].GetLocation());
				if (SquaredDistance <= SquaredRadius)
				{
					InCallback(PointRef.Index, SquaredDistance);
				}
			});
	}
}
