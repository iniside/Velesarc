// Copyright Lukasz Baran. All Rights Reserved.

#include "Forest/ArcForestField.h"

namespace ArcForestField
{
	float SampleFieldNearest(
		FVector2f Pos,
		const FBox2f& Bounds,
		TArrayView<const float> Values,
		FIntPoint Dims)
	{
		if (Values.Num() == 0 || Dims.X <= 0 || Dims.Y <= 0)
		{
			return 1.0f;
		}

		const FVector2f Size = Bounds.GetSize();

		// Degenerate axis -> clamp that coordinate to cell 0.
		const float FracX = (Size.X > KINDA_SMALL_NUMBER) ? (Pos.X - Bounds.Min.X) / Size.X : 0.0f;
		const float FracY = (Size.Y > KINDA_SMALL_NUMBER) ? (Pos.Y - Bounds.Min.Y) / Size.Y : 0.0f;

		const int32 Col = FMath::Clamp(FMath::FloorToInt(FracX * Dims.X), 0, Dims.X - 1);
		const int32 Row = FMath::Clamp(FMath::FloorToInt(FracY * Dims.Y), 0, Dims.Y - 1);

		const int32 FlatIndex = FMath::Clamp((Row * Dims.X) + Col, 0, Values.Num() - 1);
		return FMath::Clamp(Values[FlatIndex], 0.0f, 1.0f);
	}

	float SampleFieldBilinear(
		FVector2f Pos,
		const FBox2f& Bounds,
		TArrayView<const float> Values,
		FIntPoint Dims)
	{
		if (Values.Num() == 0 || Dims.X <= 0 || Dims.Y <= 0)
		{
			return 1.0f;
		}

		const FVector2f Size = Bounds.GetSize();

		// Degenerate axis -> clamp that coordinate to cell 0.
		const float FracX = (Size.X > KINDA_SMALL_NUMBER) ? (Pos.X - Bounds.Min.X) / Size.X : 0.0f;
		const float FracY = (Size.Y > KINDA_SMALL_NUMBER) ? (Pos.Y - Bounds.Min.Y) / Size.Y : 0.0f;

		// Cell-centre coordinates: centre of cell i sits at i, so the -0.5 shift puts Pos on the
		// lattice the four taps are drawn from.
		const float Fx = (FracX * static_cast<float>(Dims.X)) - 0.5f;
		const float Fy = (FracY * static_cast<float>(Dims.Y)) - 0.5f;

		// Clamping BOTH taps means the outer half-cell border constant-extrapolates, and a
		// single-cell axis (Dims == 1) collapses X0 == X1 so no interpolation happens along it.
		const int32 X0 = FMath::Clamp(FMath::FloorToInt(Fx), 0, Dims.X - 1);
		const int32 Y0 = FMath::Clamp(FMath::FloorToInt(Fy), 0, Dims.Y - 1);
		const int32 X1 = FMath::Clamp(X0 + 1, 0, Dims.X - 1);
		const int32 Y1 = FMath::Clamp(Y0 + 1, 0, Dims.Y - 1);

		const float Tx = FMath::Clamp(Fx - static_cast<float>(X0), 0.0f, 1.0f);
		const float Ty = FMath::Clamp(Fy - static_cast<float>(Y0), 0.0f, 1.0f);

		// A short Values view (fewer entries than Dims implies) clamps rather than reading OOB —
		// same defensive posture as the nearest sampler.
		const int32 LastIndex = Values.Num() - 1;
		const float V00 = Values[FMath::Clamp((Y0 * Dims.X) + X0, 0, LastIndex)];
		const float V10 = Values[FMath::Clamp((Y0 * Dims.X) + X1, 0, LastIndex)];
		const float V01 = Values[FMath::Clamp((Y1 * Dims.X) + X0, 0, LastIndex)];
		const float V11 = Values[FMath::Clamp((Y1 * Dims.X) + X1, 0, LastIndex)];

		const float Bottom = FMath::Lerp(V00, V10, Tx);
		const float Top = FMath::Lerp(V01, V11, Tx);
		return FMath::Clamp(FMath::Lerp(Bottom, Top, Ty), 0.0f, 1.0f);
	}

	FIntPoint ComputeGridDims(const FBox2f& Bounds2D, float CellSizeCm, int32 MaxGridDim)
	{
		const FVector2f Size = Bounds2D.GetSize();
		const int32 DimX = FMath::Clamp(FMath::CeilToInt(Size.X / CellSizeCm), 1, MaxGridDim);
		const int32 DimY = FMath::Clamp(FMath::CeilToInt(Size.Y / CellSizeCm), 1, MaxGridDim);
		return FIntPoint(DimX, DimY);
	}

	FVector2f CellCentre(const FBox2f& Bounds2D, FIntPoint GridDims, int32 Col, int32 Row)
	{
		const FVector2f Size = Bounds2D.GetSize();
		const float X = Bounds2D.Min.X + (static_cast<float>(Col) + 0.5f) * Size.X / static_cast<float>(GridDims.X);
		const float Y = Bounds2D.Min.Y + (static_cast<float>(Row) + 0.5f) * Size.Y / static_cast<float>(GridDims.Y);
		return FVector2f(X, Y);
	}
}

namespace ArcForestFieldLocal
{
	/**
	 * The [0,1] input value for one habitat axis, and whether that axis was supplied at all.
	 *
	 * bSupplied is what implements the response-skip rule documented on EvaluateHabitatResponse: an
	 * absent axis is skipped, never fed through the curve at a stand-in value. Kept as one return value
	 * so the switch below cannot accidentally report a value without reporting supply.
	 */
	struct FAxisSample
	{
		float Value = 0.0f;
		bool bSupplied = false;
	};

	/** A morphology grid is supplied iff it has samples AND non-degenerate dims (the sampler's own test). */
	FAxisSample SampleMorphology(const FArcForestScalarField& Field, FVector2f Pos)
	{
		FAxisSample Sample;
		if (Field.Values.Num() == 0 || Field.Dims.X <= 0 || Field.Dims.Y <= 0)
		{
			return Sample;
		}

		// BILINEAR, not nearest: the morphology grids are 200 cm/cell (coarser once MaxGridDim bites),
		// so a nearest-cell read would quantise a species' habitat edge onto visible grid steps.
		Sample.Value = Field.SampleBilinear(Pos);
		Sample.bSupplied = true;
		return Sample;
	}

	/** Resolve one axis to its [0,1] input. The ONLY switch over EArcForestHabitatField. */
	FAxisSample SampleAxis(
		const FArcForestHabitatInputs& Habitat,
		EArcForestHabitatField Field,
		FVector2f Pos,
		int32 SolveSeed)
	{
		switch (Field)
		{
		case EArcForestHabitatField::Slope:
			return SampleMorphology(Habitat.SlopeField, Pos);

		case EArcForestHabitatField::SunExposure:
			return SampleMorphology(Habitat.SunExposureField, Pos);

		case EArcForestHabitatField::Elevation:
			return SampleMorphology(Habitat.ElevationField, Pos);

		case EArcForestHabitatField::Moisture:
		{
			FAxisSample Sample;
			// Frequency <= 0 is the documented "no moisture field supplied" marker (it is what
			// FArcForestHabitatInputs::Empty() sets). SampleNoise01 would answer a flat 0.5 there, and
			// 0.5 through an authored curve is NOT neutral — so skip instead.
			if (Habitat.MoistureNoise.Frequency <= 0.0f)
			{
				return Sample;
			}
			Sample.Value = ArcForestNoise::SampleNoise01(Habitat.MoistureNoise, Pos, SolveSeed);
			Sample.bSupplied = true;
			return Sample;
		}
		}

		// Unreachable for every declared enumerator; an out-of-range value is treated as unsupplied so a
		// future axis added without an arm here degrades to neutral rather than to a hard zero.
		return FAxisSample();
	}

	/**
	 * "No habitat inputs" must mean NEUTRAL, not "default-constructed". FArcForestNoiseParams
	 * default-constructs to a LIVE Perlin field (Frequency 0.001, 3 octaves) because that is the
	 * right AUTHORED default on the node — a designer enabling a moisture response wants sensible
	 * noise, not a flat field. But the shared Empty() instance is what every call site that omits
	 * the trailing Habitat argument binds to, and those call sites mean "no self-derived habitat at
	 * all". Left default-constructed, a species with a Moisture response solved against Empty()
	 * would silently receive real noise from an input nobody supplied.
	 * Frequency 0 is the documented neutral: SampleNoise01 guards non-positive Frequency and returns
	 * 0.5, so a Moisture response against Empty() evaluates its curve at the midpoint.
	 */
	FArcForestHabitatInputs MakeNeutralHabitatInputs()
	{
		FArcForestHabitatInputs Neutral;
		Neutral.MoistureNoise.Frequency = 0.0f;
		return Neutral;
	}
}

const FArcForestHabitatInputs& FArcForestHabitatInputs::Empty()
{
	static const FArcForestHabitatInputs EmptyInputs = ArcForestFieldLocal::MakeNeutralHabitatInputs();
	return EmptyInputs;
}

namespace ArcForestField
{
	float SampleResponseLut(const FArcForestFieldResponseLut& Lut, float Value01)
	{
		return SampleLutSamples(Lut.Samples, Value01);
	}

	float SampleLutSamples(const float (&Samples)[ResponseLutSize], float Value01)
	{
		static_assert(ResponseLutSize >= 2, "A LUT needs at least two samples to interpolate between.");

		const float Clamped = FMath::Clamp(Value01, 0.0f, 1.0f);

		// Continuous index over the CLOSED interval: 0 -> sample 0, 1 -> sample (N-1), so both endpoints
		// are exact and a curve that reaches 0 at an extreme still reaches exactly 0 here.
		const float Continuous = Clamped * static_cast<float>(ResponseLutSize - 1);
		const int32 Low = FMath::Clamp(FMath::FloorToInt(Continuous), 0, ResponseLutSize - 1);
		const int32 High = FMath::Min(Low + 1, ResponseLutSize - 1);
		const float Frac = FMath::Clamp(Continuous - static_cast<float>(Low), 0.0f, 1.0f);

		return FMath::Clamp(FMath::Lerp(Samples[Low], Samples[High], Frac), 0.0f, 1.0f);
	}

	float EvaluateShadeResponse(
		const FArcForestHabitatInputs& Habitat,
		int32 SpeciesIndex,
		float Transmittance01)
	{
		// EXACT-1.0 for "no curve authored" — the shade term must be a multiplicative identity for a
		// species that expressed no preference, never a zero from an empty sample table.
		if (!Habitat.SpeciesShadeResponses.IsValidIndex(SpeciesIndex))
		{
			return 1.0f;
		}

		const FArcForestShadeResponseLut& Lut = Habitat.SpeciesShadeResponses[SpeciesIndex];
		if (!Lut.bAuthored)
		{
			return 1.0f;
		}

		return SampleLutSamples(Lut.Samples, Transmittance01);
	}

	float EvaluateHabitatResponse(
		const FArcForestHabitatInputs& Habitat,
		int32 SpeciesIndex,
		FVector2f Pos,
		int32 SolveSeed)
	{
		// EXACT-1.0 FAST PATH — the bit-identity guarantee. No species responses means no arithmetic at
		// all, so the caller's `Suitability * 1.0f` is an IEEE-754 identity and the pre-habitat seeding
		// stream is reproduced bit-for-bit.
		if (!Habitat.SpeciesResponses.IsValidIndex(SpeciesIndex))
		{
			return 1.0f;
		}

		const TArray<FArcForestFieldResponseLut>& Responses =
			Habitat.SpeciesResponses[SpeciesIndex].Responses;
		if (Responses.Num() == 0)
		{
			return 1.0f;
		}

		float Combined = 1.0f;
		for (int32 ResponseIndex = 0; ResponseIndex < Responses.Num(); ++ResponseIndex)
		{
			const FArcForestFieldResponseLut& Lut = Responses[ResponseIndex];

			const ArcForestFieldLocal::FAxisSample Axis =
				ArcForestFieldLocal::SampleAxis(Habitat, Lut.Field, Pos, SolveSeed);
			if (!Axis.bSupplied)
			{
				// Response-skip rule (see the header): an absent axis contributes exactly 1.0.
				continue;
			}

			const float CurveValue = SampleResponseLut(Lut, Axis.Value);

			// Weight == 0 and Weight == 1 are handled WITHOUT FMath::Pow. Both are exact by definition
			// (x^0 == 1, x^1 == x) and both are the common authored cases, so short-circuiting them buys
			// cross-libm reproducibility on top of the speed — powf is not required to be bit-exact at
			// integral exponents.
			if (Lut.Weight == 0.0f)
			{
				continue;
			}
			if (Lut.Weight == 1.0f)
			{
				Combined *= CurveValue;
			}
			else
			{
				Combined *= FMath::Pow(CurveValue, Lut.Weight);
			}

			// A hard zero cannot be lifted by any later factor — stop reading axes we cannot need.
			// (RNG-free by construction, so an early exit here cannot desynchronise a draw stream.)
			if (Combined <= 0.0f)
			{
				return 0.0f;
			}
		}

		return FMath::Clamp(Combined, 0.0f, 1.0f);
	}
}
