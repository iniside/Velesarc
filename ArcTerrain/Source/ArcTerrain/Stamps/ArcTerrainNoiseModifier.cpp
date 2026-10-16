// Copyright Lukasz Baran. All Rights Reserved.

#include "Stamps/ArcTerrainNoiseModifier.h"

#include "Async/ParallelFor.h"
#include "Hash/Blake3.h"
#include "Noise/ArcTerrainNoise.h"

namespace ArcTerrainNoiseModifier_Local
{
	template <typename T>
	void HashPod(FBlake3& Hash, const T& Value)
	{
		Hash.Update(&Value, sizeof(T));
	}

	/** Provider holding the field by REFCOUNT: GetSourceDescriptor runs per stamp on several per-cell
	 *  paths and the noise raster is megabytes, so a per-call copy is unaffordable. Sound because the
	 *  field is immutable and a regeneration replaces it wholesale rather than mutating it. */
	TFunction<bool(TArray<float>&, FIntPoint&)> MakeProvider(TSharedRef<const FArcTerrainBakedField> Field)
	{
		return [Field](TArray<float>& OutValues, FIntPoint& OutDims) -> bool
		{
			if (!Field->IsValid())
			{
				return false;
			}
			OutValues = Field->Values;
			OutDims = Field->Dims;
			return true;
		};
	}

	int32 ClampResolution(int32 Authored)
	{
		return FMath::Clamp(Authored, 64, 4096);
	}

	/** Deterministic content identity: the raster is a pure function of these parameters, so an
	 *  unchanged knob set MUST yield an unchanged id. That is what lets the field stay out of the map
	 *  entirely — OnRegister rebuilds it under the id the last bake already keyed, so the footprint
	 *  does not re-bake on every load. The id is folded into the cell collision key, which IS the
	 *  composite-input key the sections fold, and it reaches the persistent Zen DDC bucket through
	 *  ComputeTileCacheKey.
	 *  BUMP FormatVersion whenever ArcTerrainNoise's output or the raster layout changes: the
	 *  parameters alone would otherwise name two different rasters, and the DDC would keep serving
	 *  the old one forever because no knob edit can move the key. */
	FGuid MakeContentId(const FArcTerrainNoiseParams& Authored, int32 AuthoredResolution,
		const FVector2D& Coverage)
	{
		static const FGuid FormatVersion(0x9E41B2C7, 0x0D6A4F35, 0xB18C72E9, 0x3A5F0C41);

		// Hash what the rasteriser will actually use, never the authored values: two authored knob
		// sets that clamp together produce one raster, and two ids for it would re-bake the footprint.
		const FArcTerrainNoiseParams Params = ArcTerrainNoise::Sanitize(Authored);
		const int32 Resolution = ClampResolution(AuthoredResolution);

		FBlake3 Hash;
		HashPod(Hash, FormatVersion.A);
		HashPod(Hash, FormatVersion.B);
		HashPod(Hash, FormatVersion.C);
		HashPod(Hash, FormatVersion.D);
		HashPod(Hash, Resolution);
		HashPod(Hash, Coverage.X);
		HashPod(Hash, Coverage.Y);
		HashPod(Hash, static_cast<uint8>(Params.Basis));
		HashPod(Hash, static_cast<uint8>(Params.Fractal));
		HashPod(Hash, Params.Seed);
		HashPod(Hash, Params.FeatureSizeCm);
		HashPod(Hash, Params.AmplitudeCm);
		HashPod(Hash, Params.Octaves);
		HashPod(Hash, Params.Lacunarity);
		HashPod(Hash, Params.Gain);
		HashPod(Hash, Params.WarpStrengthCm);
		HashPod(Hash, Params.WarpFeatureSizeCm);
		HashPod(Hash, Params.TerraceStepCm);
		HashPod(Hash, Params.TerraceSharpness);
		// Field by field, never the struct: its padding bytes are uninitialised and would make the id
		// differ between runs for one knob set.
		HashPod(Hash, Params.Erosion.bEnabled);
		HashPod(Hash, Params.Erosion.ChannelDepthCm);
		HashPod(Hash, Params.Erosion.GullyWeight);
		HashPod(Hash, Params.Erosion.Detail);
		HashPod(Hash, Params.Erosion.RoundingAtValley);
		HashPod(Hash, Params.Erosion.RoundingAtPeak);
		HashPod(Hash, Params.Erosion.RoundingInputScale);
		HashPod(Hash, Params.Erosion.RoundingOctaveDecay);
		HashPod(Hash, Params.Erosion.OnsetInput);
		HashPod(Hash, Params.Erosion.OnsetOctave);
		HashPod(Hash, Params.Erosion.AssumedSlopeMagnitude);
		HashPod(Hash, Params.Erosion.AssumedSlopeBlend);
		HashPod(Hash, Params.Erosion.CellScale);
		HashPod(Hash, Params.Erosion.Octaves);
		HashPod(Hash, Params.Erosion.Gain);
		HashPod(Hash, Params.Erosion.Lacunarity);
		HashPod(Hash, Params.Erosion.Normalization);
		HashPod(Hash, Params.Erosion.ScaleCm);

		const FBlake3Hash Digest = Hash.Finalize();
		uint32 Words[4] = { 0, 0, 0, 0 };
		FMemory::Memcpy(Words, Digest.GetBytes(), sizeof(Words));
		return FGuid(Words[0], Words[1], Words[2], Words[3]);
	}

#if WITH_EDITOR
	/** The falloff knobs are absent on purpose: unlike the lake's bowl, the raster here is pure noise
	 *  over the footprint rectangle and the circle only masks it in the composite, so moving the
	 *  circle costs a recomposite but not a rebuild. */
	bool IsFieldProperty(FName PropertyName, FName MemberName)
	{
		static const FName FieldNames[] = {
			GET_MEMBER_NAME_CHECKED(UArcTerrainNoiseModifier, Noise),
			GET_MEMBER_NAME_CHECKED(UArcTerrainNoiseModifier, FieldResolution),
			GET_MEMBER_NAME_CHECKED(UArcTerrainStampComponent, CoverageXY)
		};

		for (const FName& FieldName : FieldNames)
		{
			if (PropertyName == FieldName || MemberName == FieldName)
			{
				return true;
			}
		}
		return false;
	}
#endif // WITH_EDITOR
}

UArcTerrainNoiseModifier::UArcTerrainNoiseModifier()
{
	// Noise adds relief to whatever is already composited rather than replacing it: the composite
	// reads BlendMode off the component (FArcStampSourceDesc carries no blend mode).
	BlendMode = EArcTerrainBlendMode::Additive;
	CoverageXY = FVector2D(200000.0, 200000.0);
}

void UArcTerrainNoiseModifier::OnRegister()
{
	// The field is never serialized, so EVERY load arrives here without one. Building before the base
	// registers the stamp means the first composite already sees the noise.
	if (!NoiseField.IsValid() || !NoiseField->IsValid())
	{
		BuildNoiseField();
	}

	Super::OnRegister();
}

void UArcTerrainNoiseModifier::RegenerateNoise()
{
	BuildNoiseField();
	NotifyStampChanged();
}

void UArcTerrainNoiseModifier::BuildNoiseField()
{
	using namespace ArcTerrainNoiseModifier_Local;

	const FVector2D Half = CoverageXY * 0.5;
	if (!FMath::IsFinite(CoverageXY.X) || !FMath::IsFinite(CoverageXY.Y)
		|| Half.X <= 0.0 || Half.Y <= 0.0)
	{
		NoiseField.Reset();
		return;
	}

	const int32 Resolution = ClampResolution(FieldResolution);
	// Texel (0,0) is the footprint's min corner and texel (Resolution-1, *) its max corner: the
	// composite maps the source's texel CENTRES onto the footprint edges (ComputeSampleMap).
	const double SpanX = CoverageXY.X / static_cast<double>(Resolution - 1);
	const double SpanY = CoverageXY.Y / static_cast<double>(Resolution - 1);

	// The step floor is not hashed: SpanX/SpanY fall out of CoverageXY and the clamped resolution, both
	// already in the ContentId.
	const ArcTerrainNoise::FEvaluator Evaluator(Noise, 0.5 * FMath::Min(SpanX, SpanY));

	TArray<float> Values;
	Values.SetNumUninitialized(Resolution * Resolution);
	float* const ValueData = Values.GetData();

	// Rows are disjoint slices of one preallocated buffer, so the workers need no synchronisation.
	ParallelFor(Resolution, [&Evaluator, ValueData, Resolution, SpanX, SpanY, Half](int32 TexelY)
	{
		const double PatchY = -Half.Y + static_cast<double>(TexelY) * SpanY;
		float* const Row = ValueData + static_cast<int64>(TexelY) * static_cast<int64>(Resolution);
		for (int32 TexelX = 0; TexelX < Resolution; ++TexelX)
		{
			const double PatchX = -Half.X + static_cast<double>(TexelX) * SpanX;
			Row[TexelX] = static_cast<float>(Evaluator.EvaluateCm(PatchX, PatchY));
		}
	});

	TSharedRef<FArcTerrainBakedField> Field = MakeShared<FArcTerrainBakedField>();
	Field->Values = MoveTemp(Values);
	Field->Dims = FIntPoint(Resolution, Resolution);
	Field->ContentId = MakeContentId(Noise, Resolution, CoverageXY);
	NoiseField = Field;
}

bool UArcTerrainNoiseModifier::GetSourceDescriptor(FArcStampSourceDesc& OutDesc) const
{
	if (!NoiseField.IsValid() || !NoiseField->IsValid())
	{
		return false;
	}

	OutDesc.Kind = EArcStampSourceKind::BakedField;
	OutDesc.ContentId = NoiseField->ContentId;
	OutDesc.SourceDims = NoiseField->Dims;
	OutDesc.BakedProvider = ArcTerrainNoiseModifier_Local::MakeProvider(NoiseField.ToSharedRef());
	OutDesc.SourceChannel = EArcTerrainSourceChannel::R;
	// The field stores stamp-local cm, so the composite decodes the texel unchanged.
	OutDesc.EncodingScale = 1.0f;
	OutDesc.ZeroInEncoding = 0.0f;

	OutDesc.EdgeFalloffBand = EdgeFalloffBand;
	OutDesc.EdgeSharpness = EdgeSharpness;
	OutDesc.CornerRadius = CornerRadius;
	OutDesc.bCircleMask = bCircleMask;
	OutDesc.CircleCenterOffset = CircleCenterOffset;
	OutDesc.CircleRadius = CircleRadius;
	OutDesc.CircleFalloffBand = CircleFalloffBand;
	return true;
}

#if WITH_EDITOR
void UArcTerrainNoiseModifier::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Interactive slider drags would rebuild the whole raster on every mouse delta; the release event
	// carries the final value.
	if (PropertyChangedEvent.Property != nullptr
		&& PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive
		&& ArcTerrainNoiseModifier_Local::IsFieldProperty(
			PropertyChangedEvent.GetPropertyName(), PropertyChangedEvent.GetMemberPropertyName()))
	{
		BuildNoiseField();
	}

	// Super routes the recomposite, so the new field must already be published.
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UArcTerrainNoiseModifier::PostEditUndo()
{
	// The field is not in the transaction buffer, so undo restores the knobs and leaves the raster
	// describing the parameters that were just undone. The id is a total function of those knobs, so
	// comparing it rebuilds exactly when the undo moved a raster input — undoing a move, a priority
	// or a falloff knob must not pay for a full re-rasterisation.
	const FGuid ExpectedId =
		ArcTerrainNoiseModifier_Local::MakeContentId(Noise, FieldResolution, CoverageXY);
	if (!NoiseField.IsValid() || !NoiseField->IsValid() || NoiseField->ContentId != ExpectedId)
	{
		BuildNoiseField();
	}

	Super::PostEditUndo();
}
#endif // WITH_EDITOR
