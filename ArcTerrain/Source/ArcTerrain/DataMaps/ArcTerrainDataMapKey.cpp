// Copyright Lukasz Baran. All Rights Reserved.

#include "DataMaps/ArcTerrainDataMapKey.h"

#if WITH_EDITOR

#include "ArcTerrainSubsystem.h"
#include "DataMaps/ArcTerrainDataMapBake.h"
#include "Engine/Texture2D.h"
#include "Hash/Blake3.h"
#include "Stamps/ArcTerrainStampComponent.h"
#include "Stamps/ArcTerrainTexturePatchModifier.h"
#include "Tiles/ArcTerrainSourceCache.h"

namespace ArcTerrainDataMapKey_2b74
{
	template <typename T>
	void HashPod(FBlake3& Hash, const T& Value)
	{
		Hash.Update(&Value, sizeof(T));
	}

	void HashString(FBlake3& Hash, const FString& String)
	{
		Hash.Update(*String, String.Len() * sizeof(TCHAR));
	}

	/** Per-mask density accumulator: the bake resolves each mask's quad count from its DENSEST
	 *  decodable contributor, so the key must fold the same number. */
	struct FMaskDensity
	{
		EArcTerrainDataMap Map = EArcTerrainDataMap::Deposits;
		double BestTexelsPerCm = 0.0;
		bool bAnyDecodable = false;
	};

	FMaskDensity& FindOrAddDensity(TArray<FMaskDensity>& Densities, EArcTerrainDataMap Map)
	{
		for (FMaskDensity& Existing : Densities)
		{
			if (Existing.Map == Map)
			{
				return Existing;
			}
		}
		FMaskDensity& Added = Densities.AddDefaulted_GetRef();
		Added.Map = Map;
		return Added;
	}
}

namespace ArcTerrainDataMaps
{
	void GatherDataMapKeyInputsForStamps(
		const FArcTerrainConfig& Config,
		const FTransform& TerrainTransform,
		TConstArrayView<UArcTerrainStampComponent* const> SortedStamps,
		TConstArrayView<FArcTerrainDataMapWindow> MaskWindows,
		FArcTerrainDataMapKeyInputs& OutInputs)
	{
		using namespace ArcTerrainDataMapKey_2b74;

		check(IsInGameThread());

		OutInputs.Contributions.Reset();
		OutInputs.MaskQuads.Reset();
		OutInputs.MaskWindows.Reset(MaskWindows.Num());
		for (const FArcTerrainDataMapWindow& Window : MaskWindows)
		{
			OutInputs.MaskWindows.Add(Window);
		}

		TArray<FMaskDensity> Densities;
		for (const UArcTerrainStampComponent* Stamp : SortedStamps)
		{
			const UArcTerrainTexturePatchModifier* Modifier = Cast<UArcTerrainTexturePatchModifier>(Stamp);
			if (Modifier == nullptr)
			{
				continue;
			}

			FArcStampSourceDesc Desc;
			Modifier->GetSourceDescriptor(Desc);

			const FTransform StampToTerrain =
				Modifier->GetComponentTransform().GetRelativeTransform(TerrainTransform);
			const FVector RelScale = StampToTerrain.GetScale3D();
			const FVector2D FootprintCm(
				Modifier->CoverageXY.X * FMath::Abs(RelScale.X),
				Modifier->CoverageXY.Y * FMath::Abs(RelScale.Y));

			for (const TPair<EArcTerrainDataMap, const FArcTerrainDataMapSource*>& Entry : Modifier->GetDataMapSources())
			{
				const FArcTerrainDataMapSource& Source = *Entry.Value;
				if (Source.Texture == nullptr)
				{
					continue; // the bake's own presence gate: no texture, no contributor
				}

				const FTextureSource& TextureSource = Source.Texture->Source;
				const bool bSourceValid = TextureSource.IsValid();
				const FIntPoint SourceDims = bSourceValid
					? FIntPoint(TextureSource.GetSizeX(), TextureSource.GetSizeY())
					: FIntPoint::ZeroValue;

				FArcTerrainDataMapContribution& Contribution = OutInputs.Contributions.AddDefaulted_GetRef();
				Contribution.Map = Entry.Key;
				Contribution.ModifierClassPath = Modifier->GetClass()->GetPathName();
				Contribution.Translation = StampToTerrain.GetTranslation();
				Contribution.Rotation = StampToTerrain.GetRotation();
				Contribution.Scale = RelScale;
				Contribution.CoverageXY = Modifier->CoverageXY;
				Contribution.Priority = Modifier->Priority;
				Contribution.bBlendDataMaps = Modifier->bBlendDataMaps;
				Contribution.BlendMode = static_cast<uint8>(Modifier->BlendMode);
				Contribution.ContentId = bSourceValid ? TextureSource.GetId() : FGuid();
				Contribution.SourceChannel = static_cast<uint8>(Source.SourceChannel);
				Contribution.EncodingScale = Source.EncodingScale;
				Contribution.ZeroInEncoding = Source.ZeroInEncoding;
				Contribution.SourceDims = SourceDims;
				Contribution.EdgeFalloffBand = Desc.EdgeFalloffBand;
				Contribution.EdgeSharpness = Desc.EdgeSharpness;
				Contribution.CornerRadius = Desc.CornerRadius;
				Contribution.bCircleMask = Desc.bCircleMask;
				Contribution.CircleCenterOffset = Desc.CircleCenterOffset;
				Contribution.CircleRadius = Desc.CircleRadius;
				Contribution.CircleFalloffBand = Desc.CircleFalloffBand;
				Contribution.bDrivesDensity = bSourceValid && SourceDims.X > 0 && SourceDims.Y > 0
					&& FArcTerrainSourceCache::IsDecodableSourceFormat(TextureSource.GetFormat());

				FMaskDensity& Density = FindOrAddDensity(Densities, Entry.Key);
				if (!Contribution.bDrivesDensity)
				{
					continue;
				}
				Density.bAnyDecodable = true;
				if (!FMath::IsFinite(FootprintCm.X) || !FMath::IsFinite(FootprintCm.Y)
					|| FootprintCm.X <= 0.0 || FootprintCm.Y <= 0.0)
				{
					continue;
				}
				const double TexelsPerCm = FMath::Max(
					static_cast<double>(SourceDims.X) / FootprintCm.X,
					static_cast<double>(SourceDims.Y) / FootprintCm.Y);
				if (FMath::IsFinite(TexelsPerCm) && TexelsPerCm > Density.BestTexelsPerCm)
				{
					Density.BestTexelsPerCm = TexelsPerCm;
				}
			}
		}

		Densities.Sort([](const FMaskDensity& A, const FMaskDensity& B)
		{
			return static_cast<uint8>(A.Map) < static_cast<uint8>(B.Map);
		});

		OutInputs.MaskQuads.Reserve(Densities.Num());
		for (const FMaskDensity& Density : Densities)
		{
			// No decodable contributor -> the bake emits Quads 0; anything else resolves through the
			// SAME ceiling the bake applies.
			const int32 Quads = Density.bAnyDecodable
				? ResolveMaskQuads(Config, Density.BestTexelsPerCm).Quads
				: 0;
			OutInputs.MaskQuads.Emplace(Density.Map, Quads);
		}
	}

	FIoHash ComputeCellDataKey(FArcTerrainCellKey Key, const FArcTerrainConfig& Config,
		const FArcTerrainDataMapKeyInputs& Inputs)
	{
		using namespace ArcTerrainDataMapKey_2b74;

		FBlake3 Hash;

		// OWN FormatVersionKey, DELIBERATELY DISTINCT from ComputeCellWeightKey's and
		// ComputeCellCollisionKey's: a data mask must never invalidate geometry, collision or material
		// weights, nor they it. Bump on any layout or bake-semantics change; there is no migration.
		static const FGuid FormatVersionKey(TEXT("2F4B90C1-8D63-4A7E-B015-6C3E9A2D77B4"));
		HashPod(Hash, FormatVersionKey);

		HashPod(Hash, Key.X);
		HashPod(Hash, Key.Y);
		HashPod(Hash, Config.CellSize());

		// Count FIRST so a truncated contributor set can never collide with a full one.
		const int32 NumContributions = Inputs.Contributions.Num();
		HashPod(Hash, NumContributions);
		for (const FArcTerrainDataMapContribution& Contribution : Inputs.Contributions)
		{
			// The mask enum is load-bearing: without it, moving one texture from Deposits to Wear
			// yields an identical key. Everything else is field-wise (never raw-struct — padding bytes
			// would poison the key).
			HashPod(Hash, static_cast<uint8>(Contribution.Map));
			HashString(Hash, Contribution.ModifierClassPath);
			HashPod(Hash, Contribution.Translation.X);
			HashPod(Hash, Contribution.Translation.Y);
			HashPod(Hash, Contribution.Translation.Z);
			HashPod(Hash, Contribution.Rotation.X);
			HashPod(Hash, Contribution.Rotation.Y);
			HashPod(Hash, Contribution.Rotation.Z);
			HashPod(Hash, Contribution.Rotation.W);
			HashPod(Hash, Contribution.Scale.X);
			HashPod(Hash, Contribution.Scale.Y);
			HashPod(Hash, Contribution.Scale.Z);
			HashPod(Hash, Contribution.CoverageXY.X);
			HashPod(Hash, Contribution.CoverageXY.Y);
			HashPod(Hash, Contribution.Priority);
			HashPod(Hash, Contribution.bBlendDataMaps);
			HashPod(Hash, Contribution.BlendMode);
			HashPod(Hash, Contribution.ContentId);
			HashPod(Hash, Contribution.SourceChannel);
			HashPod(Hash, Contribution.EncodingScale);
			HashPod(Hash, Contribution.ZeroInEncoding);
			HashPod(Hash, Contribution.EdgeFalloffBand);
			HashPod(Hash, Contribution.EdgeSharpness);
			HashPod(Hash, Contribution.CornerRadius);
			HashPod(Hash, Contribution.bCircleMask);
			HashPod(Hash, Contribution.CircleCenterOffset.X);
			HashPod(Hash, Contribution.CircleCenterOffset.Y);
			HashPod(Hash, Contribution.CircleRadius);
			HashPod(Hash, Contribution.CircleFalloffBand);
			HashPod(Hash, Contribution.SourceDims.X);
			HashPod(Hash, Contribution.SourceDims.Y);
		}

		// The per-mask raster the payload is stored at: it folds CellSize, the byte ceiling cvar and the
		// densest contributor, none of which the fields above pin down on their own.
		const int32 NumMasks = Inputs.MaskQuads.Num();
		HashPod(Hash, NumMasks);
		for (const TPair<EArcTerrainDataMap, int32>& MaskQuads : Inputs.MaskQuads)
		{
			HashPod(Hash, static_cast<uint8>(MaskQuads.Key));
			HashPod(Hash, MaskQuads.Value);
		}

		// The quantization windows are TERRAIN-WIDE, so a modifier this cell never sees still moves the
		// bytes stored here. Without them a widened window would re-encode only the tiles it overlaps and
		// the two encodings would meet at a seam.
		const int32 NumWindows = Inputs.MaskWindows.Num();
		HashPod(Hash, NumWindows);
		for (const FArcTerrainDataMapWindow& Window : Inputs.MaskWindows)
		{
			HashPod(Hash, static_cast<uint8>(Window.Map));
			HashPod(Hash, Window.Min);
			HashPod(Hash, Window.Max);
		}

		return Hash.Finalize();
	}
}

#endif // WITH_EDITOR
