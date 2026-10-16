// Copyright Lukasz Baran. All Rights Reserved.

#include "Forest/ArcForestSpeciesDefinition.h"

#include "Curves/CurveFloat.h"
#include "Forest/ArcForestField.h"

namespace ArcForestSpeciesDefinitionLocal
{
	/**
	 * True when the authored curve carries at least one key on whichever source it resolves to.
	 *
	 * This is the DROP test, and it exists because FRichCurve::Eval on a keyless curve returns the
	 * caller-supplied default (0 here), not "no opinion". A species whose designer added a response row
	 * but never drew a curve would therefore be multiplied by 0 at every position and vanish from the
	 * whole volume — a silent, total exclusion caused by a half-finished edit. Dropping the response
	 * instead makes an unauthored row a no-op, which is what a half-finished edit should be.
	 *
	 * FRuntimeFloatCurve resolves to ExternalCurve->FloatCurve when an external asset is assigned and to
	 * the inline EditorCurveData otherwise (Curves/CurveFloat.h), so testing the resolved curve covers
	 * both sources.
	 */
	bool HasAuthoredKeys(const FRuntimeFloatCurve& Curve)
	{
		const FRichCurve* Rich = Curve.GetRichCurveConst();
		return Rich != nullptr && Rich->GetNumKeys() > 0;
	}

	/**
	 * Resample an authored curve across the CLOSED [0,1] interval into a LUT sample table — the single
	 * copy of the flatten arithmetic, shared by the habitat field responses and the recruitment shade
	 * response so the two cannot drift in indexing or clamping.
	 *
	 * Index 0 is the curve at 0.0 and the last index the curve at 1.0 exactly, which is what lets a
	 * curve deliberately reaching 0 at an extreme still reach exactly 0 here, and it matches
	 * ArcForestField::SampleLutSamples' indexing.
	 */
	void FlattenCurveToSamples(const FRichCurve& Rich, float (&OutSamples)[ArcForestField::ResponseLutSize])
	{
		for (int32 SampleIndex = 0; SampleIndex < ArcForestField::ResponseLutSize; ++SampleIndex)
		{
			const float Input =
				static_cast<float>(SampleIndex) / static_cast<float>(ArcForestField::ResponseLutSize - 1);
			OutSamples[SampleIndex] = FMath::Clamp(Rich.Eval(Input, 0.0f), 0.0f, 1.0f);
		}
	}
}

namespace ArcForestSpeciesHabitat
{
	void FlattenFieldResponses(
		const UArcForestSpeciesDefinition& Species,
		FArcForestHabitatInputs& OutHabitat)
	{
		const TArray<FArcForestSpeciesEntry>& Table = Species.Species;

		// Fully overwritten, and always sized to the species count so every species index is valid —
		// EvaluateHabitatResponse's IsValidIndex fast path then only fires for a genuinely absent
		// bundle, not for a species that merely has no preferences.
		OutHabitat.SpeciesResponses.Reset();
		OutHabitat.SpeciesResponses.SetNum(Table.Num());

		// Same sizing rule for the recruitment shade LUTs: every species index is valid, and a species
		// that authored no curve simply has bAuthored == false (the exact-1.0 neutral).
		OutHabitat.SpeciesShadeResponses.Reset();
		OutHabitat.SpeciesShadeResponses.SetNum(Table.Num());

		for (int32 SpeciesIndex = 0; SpeciesIndex < Table.Num(); ++SpeciesIndex)
		{
			// Recruitment shade preference: transmittance -> preference. A keyless curve is UNAUTHORED
			// and stays neutral, exactly as a keyless habitat curve is dropped rather than evaluated as 0.
			const FRuntimeFloatCurve& ShadeCurve = Table[SpeciesIndex].ShadeResponseCurve;
			if (ArcForestSpeciesDefinitionLocal::HasAuthoredKeys(ShadeCurve))
			{
				FArcForestShadeResponseLut& ShadeLut = OutHabitat.SpeciesShadeResponses[SpeciesIndex];
				ArcForestSpeciesDefinitionLocal::FlattenCurveToSamples(
					*ShadeCurve.GetRichCurveConst(), ShadeLut.Samples);
				ShadeLut.bAuthored = true;
			}

			const TArray<FArcForestFieldResponse>& Authored = Table[SpeciesIndex].FieldResponses;
			TArray<FArcForestFieldResponseLut>& Flattened = OutHabitat.SpeciesResponses[SpeciesIndex].Responses;
			Flattened.Reset();
			Flattened.Reserve(Authored.Num());

			for (int32 ResponseIndex = 0; ResponseIndex < Authored.Num(); ++ResponseIndex)
			{
				const FArcForestFieldResponse& Response = Authored[ResponseIndex];
				if (!ArcForestSpeciesDefinitionLocal::HasAuthoredKeys(Response.ResponseCurve))
				{
					continue;
				}

				const FRichCurve* Rich = Response.ResponseCurve.GetRichCurveConst();

				FArcForestFieldResponseLut Lut;
				Lut.Field = Response.Field;

				// Clamped here so the worker never has to defend against an authored value: a negative
				// Weight would invert the response (Pow with a negative exponent diverges as the curve
				// value approaches 0), and Pow(x, 0) == 1 is the documented no-op.
				Lut.Weight = FMath::Max(Response.Weight, 0.0f);

				// Sampled across the CLOSED interval (see FlattenCurveToSamples): index 0 is the curve at
				// 0.0 and the last index the curve at 1.0 exactly, matching SampleResponseLut's indexing.
				ArcForestSpeciesDefinitionLocal::FlattenCurveToSamples(*Rich, Lut.Samples);

				Flattened.Add(MoveTemp(Lut));
			}
		}
	}
}
