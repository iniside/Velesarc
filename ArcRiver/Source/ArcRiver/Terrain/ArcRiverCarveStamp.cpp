// Copyright Lukasz Baran. All Rights Reserved.

#include "Terrain/ArcRiverCarveStamp.h"

#include "Engine/Texture2D.h"
#include "Fields/ArcTerrainMaskDistance.h"
#include "GameFramework/Actor.h"
#include "Hash/Blake3.h"
#include "Stamps/ArcTerrainTexturePatchModifier.h"
#include "Tiles/ArcTerrainSourceCache.h"

namespace ArcRiverCarveStampPrivate
{
	/** Just above the 100.0 the texture patch modifier keeps: GatherSortedStamps composites ascending,
	 *  so the carve must sort after the Gaea heightmap whose channels it deepens. */
	constexpr double CarvePriority = 110.0;

	constexpr int32 MinCarveResolution = 64;
	constexpr int32 MaxCarveResolution = 2048;

	template <typename T>
	void HashField(FBlake3& Hash, const T& Value)
	{
		Hash.Update(&Value, sizeof(T));
	}

	FGuid FinalizeGuid(FBlake3& Hash)
	{
		const FBlake3Hash Digest = Hash.Finalize();
		uint32 Words[4] = { 0, 0, 0, 0 };
		FMemory::Memcpy(Words, Digest.GetBytes(), sizeof(Words));
		// An all-zero guid reads as "no content" to the source cache, which drops the stamp silently.
		Words[0] |= 1u;
		return FGuid(Words[0], Words[1], Words[2], Words[3]);
	}

	/** Everything the field is a pure function of. Held by shared ref so a descriptor copies a pointer. */
	struct FCarveParams
	{
		TWeakObjectPtr<UTexture2D> MaskTexture;
		EArcTerrainSourceChannel MaskChannel = EArcTerrainSourceChannel::R;
		float MaskEncodingScale = 1.0f;
		float MaskZeroInEncoding = 0.0f;
		float CarveDepth = 0.0f;
		float BankSoftness = 0.0f;
		int32 Resolution = MinCarveResolution;
		FVector2D CoverageXY = FVector2D::ZeroVector;
	};

	FGuid MakeContentId(const FCarveParams& Params, const FGuid& MaskSourceId)
	{
		static const FGuid FormatVersion(0x2E71A93C, 0x4B0D4F16, 0xA5C38E42, 0x6D19B7F0);

		// Field by field, never the whole struct: padding bytes are uninitialised and would key a
		// raster that has to be reproducible across sessions.
		FBlake3 Hash;
		HashField(Hash, FormatVersion.A);
		HashField(Hash, FormatVersion.B);
		HashField(Hash, FormatVersion.C);
		HashField(Hash, FormatVersion.D);
		HashField(Hash, MaskSourceId.A);
		HashField(Hash, MaskSourceId.B);
		HashField(Hash, MaskSourceId.C);
		HashField(Hash, MaskSourceId.D);
		HashField(Hash, static_cast<uint8>(Params.MaskChannel));
		HashField(Hash, Params.MaskEncodingScale);
		HashField(Hash, Params.MaskZeroInEncoding);
		HashField(Hash, Params.CarveDepth);
		HashField(Hash, Params.BankSoftness);
		HashField(Hash, Params.Resolution);
		HashField(Hash, Params.CoverageXY.X);
		HashField(Hash, Params.CoverageXY.Y);
		return FinalizeGuid(Hash);
	}

	int32 ResolveResolution(int32 Authored)
	{
		return FMath::Clamp(Authored, MinCarveResolution, MaxCarveResolution);
	}

#if WITH_EDITORONLY_DATA
	/** Decoded Rivers value at which a texel counts as channel rather than dry ground. */
	constexpr double WaterMaskThreshold = 0.5;

	/** Source texels covered by one carve texel, on one axis. Texel 0 sits on the footprint's min edge
	 *  and texel Resolution-1 on its max edge, matching the composite's ComputeSampleMap. */
	void BuildAxisRanges(int32 Resolution, int32 SourceSize, TArray<int32>& OutFirst, TArray<int32>& OutLast)
	{
		const double SourceSpan = static_cast<double>(SourceSize - 1) / static_cast<double>(Resolution - 1);
		OutFirst.SetNumUninitialized(Resolution);
		OutLast.SetNumUninitialized(Resolution);
		for (int32 Index = 0; Index < Resolution; ++Index)
		{
			const double Center = static_cast<double>(Index) * SourceSpan;
			int32 First = FMath::Clamp(FMath::CeilToInt32(Center - 0.5 * SourceSpan), 0, SourceSize - 1);
			int32 Last = FMath::Clamp(FMath::FloorToInt32(Center + 0.5 * SourceSpan), 0, SourceSize - 1);
			if (Last < First)
			{
				First = FMath::Clamp(FMath::RoundToInt32(Center), 0, SourceSize - 1);
				Last = First;
			}
			OutFirst[Index] = First;
			OutLast[Index] = Last;
		}
	}

	bool BuildCarveField(const FCarveParams& Params, TArray<float>& OutValues, FIntPoint& OutDims)
	{
		UTexture2D* const MaskTexture = Params.MaskTexture.Get();
		if (MaskTexture == nullptr)
		{
			return false;
		}

		const FTextureSource& Source = MaskTexture->Source;
		if (!Source.IsValid() || !FArcTerrainSourceCache::IsDecodableSourceFormat(Source.GetFormat()))
		{
			return false;
		}

		FArcTerrainSourceCache Cache;
		const FArcTerrainSourceCpuFloatsPtr Floats =
			Cache.GetOrCreateCpuFloatsFromTexture(*MaskTexture, Params.MaskChannel, Source.GetId());
		if (!Floats.IsValid() || Floats->Dims.X < 2 || Floats->Dims.Y < 2)
		{
			return false;
		}

		const int32 Resolution = ResolveResolution(Params.Resolution);
		const int32 SourceWidth = Floats->Dims.X;
		const int32 SourceHeight = Floats->Dims.Y;

		TArray<int32> FirstX;
		TArray<int32> LastX;
		TArray<int32> FirstY;
		TArray<int32> LastY;
		BuildAxisRanges(Resolution, SourceWidth, FirstX, LastX);
		BuildAxisRanges(Resolution, SourceHeight, FirstY, LastY);

		// Distance is measured to the DRY cells, so a channel texel's distance is its distance to the
		// bank. A carve texel counts as channel when any source texel it covers does — downsampling by
		// coverage average would erase a one-texel creek instead of narrowing it.
		TArray<uint8> DryMask;
		DryMask.SetNumUninitialized(Resolution * Resolution);
		for (int32 TexelY = 0; TexelY < Resolution; ++TexelY)
		{
			for (int32 TexelX = 0; TexelX < Resolution; ++TexelX)
			{
				bool bWater = false;
				for (int32 SourceY = FirstY[TexelY]; SourceY <= LastY[TexelY] && !bWater; ++SourceY)
				{
					for (int32 SourceX = FirstX[TexelX]; SourceX <= LastX[TexelX]; ++SourceX)
					{
						const double Decoded =
							(static_cast<double>(Floats->Values[SourceX + SourceY * SourceWidth])
								- static_cast<double>(Params.MaskZeroInEncoding))
							* static_cast<double>(Params.MaskEncodingScale);
						if (Decoded >= WaterMaskThreshold)
						{
							bWater = true;
							break;
						}
					}
				}
				DryMask[TexelX + TexelY * Resolution] = bWater ? 0 : 1;
			}
		}

		TArray<double> SquaredCellDistances;
		ArcTerrainMaskDistance::BuildSquaredCellDistanceToMask(
			DryMask, Resolution, Resolution, SquaredCellDistances);

		const double SpanX = Params.CoverageXY.X / static_cast<double>(Resolution - 1);
		const double SpanY = Params.CoverageXY.Y / static_cast<double>(Resolution - 1);
		const double CmPerCell = 0.5 * (SpanX + SpanY);
		const double Softness = FMath::Max(static_cast<double>(Params.BankSoftness), 0.0);
		const double Depth = FMath::Max(static_cast<double>(Params.CarveDepth), 0.0);

		OutValues.SetNumUninitialized(Resolution * Resolution);
		for (int32 Index = 0; Index < SquaredCellDistances.Num(); ++Index)
		{
			const double DistanceCm = FMath::Sqrt(FMath::Max(SquaredCellDistances[Index], 0.0)) * CmPerCell;
			double Ramp = 0.0;
			if (DistanceCm > 0.0)
			{
				Ramp = Softness > 0.0 ? FMath::Clamp(DistanceCm / Softness, 0.0, 1.0) : 1.0;
				Ramp = Ramp * Ramp * (3.0 - 2.0 * Ramp);
			}
			OutValues[Index] = static_cast<float>(-Depth * Ramp);
		}

		OutDims = FIntPoint(Resolution, Resolution);
		return true;
	}

	/** The provider holds its inputs by shared ref because GetSourceDescriptor runs per stamp on several
	 *  per-cell paths: a descriptor must copy a pointer, never a raster. */
	TFunction<bool(TArray<float>&, FIntPoint&)> MakeProvider(TSharedRef<const FCarveParams> Params)
	{
		return [Params](TArray<float>& OutValues, FIntPoint& OutDims) -> bool
		{
			return BuildCarveField(*Params, OutValues, OutDims);
		};
	}
#endif // WITH_EDITORONLY_DATA
}

UArcRiverCarveStamp::UArcRiverCarveStamp()
{
	// Additive is the one mode the composite zeroes ValueBias for (Height += Value * Mask), which makes
	// the field a SIGNED delta: a negative value lowers the surface by that many cm wherever it already
	// is, with no absolute heights and no ground sampling.
	BlendMode = EArcTerrainBlendMode::Additive;
	Priority = ArcRiverCarveStampPrivate::CarvePriority;
}

void UArcRiverCarveStamp::OnRegister()
{
	// Before the base registers: registration snapshots ComputeBounds(), which reads the footprint.
	SyncFootprintFromPatchModifier();

	Super::OnRegister();
}

bool UArcRiverCarveStamp::GetSourceDescriptor(FArcStampSourceDesc& OutDesc) const
{
#if WITH_EDITORONLY_DATA
	using namespace ArcRiverCarveStampPrivate;

	if (CarveDepth <= 0.0f)
	{
		return false;
	}

	const UArcTerrainTexturePatchModifier* const Patch = FindPatchModifier();
	if (Patch == nullptr || Patch->Rivers.Texture == nullptr)
	{
		return false;
	}

	const FTextureSource& Source = Patch->Rivers.Texture->Source;
	if (!Source.IsValid() || !FArcTerrainSourceCache::IsDecodableSourceFormat(Source.GetFormat()))
	{
		return false;
	}

	TSharedRef<FCarveParams> Params = MakeShared<FCarveParams>();
	Params->MaskTexture = Patch->Rivers.Texture;
	Params->MaskChannel = Patch->Rivers.SourceChannel;
	Params->MaskEncodingScale = Patch->Rivers.EncodingScale;
	Params->MaskZeroInEncoding = Patch->Rivers.ZeroInEncoding;
	Params->CarveDepth = CarveDepth;
	Params->BankSoftness = BankSoftness;
	Params->Resolution = ResolveResolution(CarveResolution);
	Params->CoverageXY = CoverageXY;

	OutDesc.Kind = EArcStampSourceKind::BakedField;
	OutDesc.ContentId = MakeContentId(*Params, Source.GetId());
	OutDesc.SourceDims = FIntPoint(Params->Resolution, Params->Resolution);
	OutDesc.BakedProvider = MakeProvider(Params);
	OutDesc.SourceChannel = EArcTerrainSourceChannel::R;
	// The field is already stamp-local cm.
	OutDesc.EncodingScale = 1.0f;
	OutDesc.ZeroInEncoding = 0.0f;
	return true;
#else
	return false;
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITOR
void UArcRiverCarveStamp::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Interactive slider drags would rebuild the whole raster on every mouse delta; the release event
	// carries the final value.
	if (PropertyChangedEvent.Property != nullptr
		&& PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		// Before Super, which routes the recomposite off the footprint this may have just moved.
		SyncFootprintFromPatchModifier();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

const UArcTerrainTexturePatchModifier* UArcRiverCarveStamp::FindPatchModifier() const
{
	const AActor* const Owner = GetOwner();
	if (Owner == nullptr)
	{
		return nullptr;
	}

	TArray<UArcTerrainTexturePatchModifier*> Found;
	Owner->GetComponents<UArcTerrainTexturePatchModifier>(Found);
	return Found.Num() == 1 ? Found[0] : nullptr;
}

void UArcRiverCarveStamp::SyncFootprintFromPatchModifier()
{
	const UArcTerrainTexturePatchModifier* const Patch = FindPatchModifier();
	if (Patch == nullptr)
	{
		return;
	}

	// Relative, not world: the relative transform is serialized state and is therefore already correct
	// whichever sibling registers first.
	const FTransform PatchRelativeTransform = Patch->GetRelativeTransform();
	const bool bTransformDiffers = !GetRelativeTransform().Equals(PatchRelativeTransform);
	if (AffectedTerrain == Patch->AffectedTerrain && CoverageXY == Patch->CoverageXY && !bTransformDiffers)
	{
		return;
	}

	AffectedTerrain = Patch->AffectedTerrain;
	CoverageXY = Patch->CoverageXY;
	if (bTransformDiffers)
	{
		SetRelativeTransform(PatchRelativeTransform);
	}
}
