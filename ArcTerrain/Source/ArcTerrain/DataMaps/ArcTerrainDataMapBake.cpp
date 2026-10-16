// Copyright Lukasz Baran. All Rights Reserved.

#include "DataMaps/ArcTerrainDataMapBake.h"

#if WITH_EDITOR

#include "ArcTerrainModule.h"
#include "ArcTerrainSubsystem.h"
#include "Composite/ArcTerrainStampMask.h"
#include "Engine/Texture2D.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Hash/Blake3.h"
#include "IO/IoHash.h"
#include "Stamps/ArcTerrainStampComponent.h"
#include "Stamps/ArcTerrainTexturePatchModifier.h"
#include "Tiles/ArcTerrainSourceCache.h"

static TAutoConsoleVariable<int32> CVarArcTerrainDataMapMaxBytesPerMaskPerCell(
	TEXT("arc.Terrain.DataMaps.MaxBytesPerMaskPerCell"),
	1 << 20,
	TEXT("Ceiling (bytes) on ONE data mask's per-cell payload. A mask whose source density resolves ")
	TEXT("above it is downsampled to the largest quad count that fits, warning once per bake pass."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarArcTerrainDataMapBakeStats(
	TEXT("arc.Terrain.DataMaps.BakeStats"),
	0,
	TEXT("Diagnostics only. 1 logs one ArcTerrainBakeStats line per data mask tile bake — per mask the ")
	TEXT("contributor count, the summed contributor texel-rect area and the written-texel count as ")
	TEXT("percentages of the tile, and the gather/windows/accumulate/quantize/stats/other ")
	TEXT("millisecond split of the tile total — plus one ")
	TEXT("ArcTerrainRebakeReason line per tile that missed its decision. fused=N marks N masks rastered ")
	TEXT("together over one shared stamp geometry: their quantize cost sits inside accum_ms, charged ")
	TEXT("once to the group's first mask, and quant_ms is 0 for all of them. 0 costs nothing."),
	ECVF_Default);

namespace ArcTerrainDataMapBake_5c1e
{
	/** Tile edge stays <= 4093 texels, the cap the weight raster resolves under. */
	constexpr int32 MaxMaskQuads = 4090;

	/** Slack on the normalized value before a clamp counts as saturation, so float noise at the window's
	 *  own bounds does not warn. */
	constexpr float SaturationTolerance = 1.0e-4f;

	using FContributor = FArcTerrainDataMapTileSnapshot::FContributor;
	using FMaskWork = FArcTerrainDataMapTileSnapshot::FMaskWork;

	template <typename PodType>
	void HashPod(FBlake3& Hash, const PodType& Value)
	{
		Hash.Update(&Value, sizeof(PodType));
	}

	FIoHash ComputeMaskWindowFingerprint(TConstArrayView<ArcTerrainDataMaps::FArcTerrainDataMapWindow> Windows)
	{
		FBlake3 Hash;
		const int32 NumWindows = Windows.Num();
		HashPod(Hash, NumWindows);
		for (const ArcTerrainDataMaps::FArcTerrainDataMapWindow& Window : Windows)
		{
			HashPod(Hash, static_cast<uint8>(Window.Map));
			HashPod(Hash, Window.Min);
			HashPod(Hash, Window.Max);
		}
		return Hash.Finalize();
	}

	using ArcTerrainSubsystemPrivate::ComputeSampleMap;

	float SampleBilinearClamped(const FArcTerrainSourceCpuFloats& Floats, const FVector2f& UV)
	{
		const int32 Width = Floats.Dims.X;
		const int32 Height = Floats.Dims.Y;
		const float X = UV.X * static_cast<float>(Width) - 0.5f;
		const float Y = UV.Y * static_cast<float>(Height) - 0.5f;
		const float FloorX = FMath::FloorToFloat(X);
		const float FloorY = FMath::FloorToFloat(Y);
		const int32 IX0 = FMath::FloorToInt32(X);
		const int32 IY0 = FMath::FloorToInt32(Y);
		const int32 X0 = FMath::Clamp(IX0, 0, Width - 1);
		const int32 Y0 = FMath::Clamp(IY0, 0, Height - 1);
		const int32 X1 = FMath::Clamp(IX0 + 1, 0, Width - 1);
		const int32 Y1 = FMath::Clamp(IY0 + 1, 0, Height - 1);
		const float FracX = FMath::Clamp(X - FloorX, 0.0f, 1.0f);
		const float FracY = FMath::Clamp(Y - FloorY, 0.0f, 1.0f);

		const float V00 = Floats.Values[static_cast<int64>(Y0) * Width + X0];
		const float V10 = Floats.Values[static_cast<int64>(Y0) * Width + X1];
		const float V01 = Floats.Values[static_cast<int64>(Y1) * Width + X0];
		const float V11 = Floats.Values[static_cast<int64>(Y1) * Width + X1];
		return FMath::Lerp(FMath::Lerp(V00, V10, FracX), FMath::Lerp(V01, V11, FracX), FracY);
	}

	// A replacing contributor overwrites through the falloff mask, so a falloff band fades back to
	// the earlier value instead of to zero; only the FIRST write folds against nothing.
	float FoldValue(const FContributor& Contributor, bool bWritten, float Accum, float Value, float Mask)
	{
		if (!bWritten)
		{
			return Value * Mask;
		}
		if (!Contributor.bBlendDataMaps)
		{
			return FMath::Lerp(Accum, Value, Mask);
		}
		switch (Contributor.BlendMode)
		{
		case EArcTerrainBlendMode::Additive:
			return Accum + Value * Mask;
		case EArcTerrainBlendMode::AlphaBlend:
			return FMath::Lerp(Accum, Value, Mask);
		case EArcTerrainBlendMode::Min:
			return FMath::Lerp(Accum, FMath::Min(Accum, Value), Mask);
		case EArcTerrainBlendMode::Max:
			return FMath::Lerp(Accum, FMath::Max(Accum, Value), Mask);
		default:
			return Accum;
		}
	}

	FMaskWork& FindOrAddMaskWork(TArray<FMaskWork>& Work, EArcTerrainDataMap Map)
	{
		for (FMaskWork& Existing : Work)
		{
			if (Existing.Map == Map)
			{
				return Existing;
			}
		}
		FMaskWork& Added = Work.AddDefaulted_GetRef();
		Added.Map = Map;
		return Added;
	}

	ArcTerrainDataMaps::FArcTerrainDataMapWindow& FindOrAddWindow(
		TArray<ArcTerrainDataMaps::FArcTerrainDataMapWindow>& Windows, EArcTerrainDataMap Map)
	{
		for (ArcTerrainDataMaps::FArcTerrainDataMapWindow& Existing : Windows)
		{
			if (Existing.Map == Map)
			{
				return Existing;
			}
		}
		ArcTerrainDataMaps::FArcTerrainDataMapWindow& Added = Windows.AddDefaulted_GetRef();
		Added.Map = Map;
		return Added;
	}

	FString MaskName(EArcTerrainDataMap Map)
	{
		return StaticEnum<EArcTerrainDataMap>()->GetNameStringByValue(static_cast<int64>(Map));
	}

	using FMaskStat = FArcTerrainDataMapBakeMaskStats;

	FString FormatMaskStats(const TArray<FMaskStat>& Stats, int32 TexelCount)
	{
		const double PercentPerTexel = TexelCount > 0 ? 100.0 / static_cast<double>(TexelCount) : 0.0;
		FString Result;
		for (const FMaskStat& Stat : Stats)
		{
			Result += FString::Printf(
				TEXT("{mask=%s contrib=%d fused=%d rect_pct=%.2f covered_pct=%.2f accum_ms=%.3f ")
				TEXT("quant_ms=%.3f}"),
				*MaskName(Stat.Map), Stat.Contributors, Stat.FusedMasks,
				static_cast<double>(Stat.RectAreaSum) * PercentPerTexel,
				static_cast<double>(Stat.CoveredTexels) * PercentPerTexel,
				Stat.AccumMs, Stat.QuantMs);
		}
		return Result;
	}

	void GatherMaskWork(const FTransform& TerrainTransform,
		TConstArrayView<UArcTerrainStampComponent* const> SortedStamps,
		FArcTerrainDataMapBakeScope& Scope,
		TArray<FMaskWork>& OutWork)
	{
		OutWork.Reset();

		for (int32 StampIndex = 0; StampIndex < SortedStamps.Num(); ++StampIndex)
		{
			const UArcTerrainStampComponent* Stamp = SortedStamps[StampIndex];
			const UArcTerrainTexturePatchModifier* Modifier = Cast<UArcTerrainTexturePatchModifier>(Stamp);
			if (Modifier == nullptr)
			{
				continue;
			}

			FArcStampSourceDesc Desc;
			Modifier->GetSourceDescriptor(Desc);

			const FTransform StampToTerrain =
				Modifier->GetComponentTransform().GetRelativeTransform(TerrainTransform);
			const FMatrix LocalToPatch = StampToTerrain.ToInverseMatrixWithScale();
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

				FMaskWork& Mask = FindOrAddMaskWork(OutWork, Entry.Key);

				FArcTerrainSourceCpuFloatsPtr Floats;
				if (Source.Texture->Source.IsValid())
				{
					const FGuid ContentId = Source.Texture->Source.GetId();
					const double RiverSourceStart = FPlatformTime::Seconds();
					UE_LOGF(LogArcTerrain, Display,"[RiverGen] SOURCE DECODE BEGIN stamp=%ls texture=%ls channel=%d t=%.6f", *Modifier->GetPathName(), *Source.Texture->GetPathName(), static_cast<int32>(Source.SourceChannel), RiverSourceStart);
					Floats = Scope.SourceCache.GetOrCreateCpuFloatsFromTexture(
						*Source.Texture, Source.SourceChannel, ContentId);
					UE_LOGF(LogArcTerrain, Display,"[RiverGen] SOURCE DECODE END valid=%d dims=%ls elapsed=%.3fs", Floats.IsValid(), Floats.IsValid() ? *Floats->Dims.ToString() : TEXT("none"), FPlatformTime::Seconds() - RiverSourceStart);
					if (Floats.IsValid())
					{
						Scope.SourceCache.PinCpuFloats(ContentId, Source.SourceChannel);
					}
				}
				if (!Floats.IsValid() || Floats->Dims.X <= 0 || Floats->Dims.Y <= 0)
				{
					Mask.bUnresolved = true;
					const FString WarnKey = FString::Printf(TEXT("%d|%s"),
						static_cast<int32>(Entry.Key), *Modifier->GetPathName());
					if (!Scope.WarnedUnresolvedSources.Contains(WarnKey))
					{
						Scope.WarnedUnresolvedSources.Add(WarnKey);
						UE_LOGF(LogArcTerrain, Warning,
							"ArcTerrain data map '%ls': stamp '%ls' texture '%ls' did not decode; the mask is "
							"UNRESOLVED for every cell it covers (never folded as zeros).",
							*MaskName(Entry.Key), *Modifier->GetPathName(), *Source.Texture->GetName());
					}
					continue;
				}

				FContributor& Contributor = Mask.Contributors.AddDefaulted_GetRef();
				Contributor.Source = Floats;
				Contributor.LocalToPatch = LocalToPatch;
				Contributor.CoverageXY = Modifier->CoverageXY;
				Contributor.FootprintCm = FootprintCm;
				ComputeSampleMap(Floats->Dims, Contributor.UVScale, Contributor.UVBias);
				Contributor.EncodingScale = Source.EncodingScale;
				Contributor.ZeroInEncoding = Source.ZeroInEncoding;
				Contributor.bBlendDataMaps = Modifier->bBlendDataMaps;
				Contributor.BlendMode = Modifier->BlendMode;
				Contributor.Mask.EdgeFalloffBand = Desc.EdgeFalloffBand;
				Contributor.Mask.EdgeSharpness = Desc.EdgeSharpness;
				Contributor.Mask.CornerRadius = Desc.CornerRadius;
				Contributor.Mask.bCircleMask = Desc.bCircleMask;
				Contributor.Mask.CircleCenterOffset = FVector2f(Desc.CircleCenterOffset);
				Contributor.Mask.CircleRadius = Desc.CircleRadius;
				Contributor.Mask.CircleFalloffBand = Desc.CircleFalloffBand;
				Contributor.GeometryIdentity = static_cast<uint32>(StampIndex);
			}
		}

		OutWork.Sort([](const FMaskWork& A, const FMaskWork& B)
		{
			return static_cast<uint8>(A.Map) < static_cast<uint8>(B.Map);
		});
	}

	/** Texel rect (Max exclusive) outside which one contributor's patch rect cannot contain a texel, or
	 *  false when its footprint misses the raster. Inverts the 2x2 block the z = 0 slab actually travels
	 *  through, NOT the 4x4: the inside test ignores patch z, so a full inverse bounds the wrong set. */
	bool ComputeContributorTexelRect(const FContributor& Contributor, FIntPoint TexelOrigin,
		double TexelToLocal, int32 Resolution, FIntRect& OutRect)
	{
		const FMatrix& M = Contributor.LocalToPatch;
		const double Determinant = M.M[0][0] * M.M[1][1] - M.M[0][1] * M.M[1][0];
		const double HalfX = static_cast<double>(static_cast<float>(Contributor.CoverageXY.X * 0.5));
		const double HalfY = static_cast<double>(static_cast<float>(Contributor.CoverageXY.Y * 0.5));

		double MinLocalX = TNumericLimits<double>::Max();
		double MinLocalY = TNumericLimits<double>::Max();
		double MaxLocalX = TNumericLimits<double>::Lowest();
		double MaxLocalY = TNumericLimits<double>::Lowest();
		bool bBounded = FMath::IsFinite(Determinant) && FMath::Abs(Determinant) > UE_DOUBLE_SMALL_NUMBER
			&& FMath::IsFinite(HalfX) && FMath::IsFinite(HalfY);
		for (int32 Corner = 0; bBounded && Corner < 4; ++Corner)
		{
			const double U = ((Corner & 1) != 0 ? HalfX : -HalfX) - M.M[3][0];
			const double V = ((Corner & 2) != 0 ? HalfY : -HalfY) - M.M[3][1];
			const double LocalX = (U * M.M[1][1] - V * M.M[1][0]) / Determinant;
			const double LocalY = (V * M.M[0][0] - U * M.M[0][1]) / Determinant;
			bBounded = FMath::IsFinite(LocalX) && FMath::IsFinite(LocalY);
			MinLocalX = FMath::Min(MinLocalX, LocalX);
			MinLocalY = FMath::Min(MinLocalY, LocalY);
			MaxLocalX = FMath::Max(MaxLocalX, LocalX);
			MaxLocalY = FMath::Max(MaxLocalY, LocalY);
		}
		if (!bBounded)
		{
			OutRect = FIntRect(0, 0, Resolution, Resolution);
			return true;
		}

		const double MinColD = FMath::FloorToDouble(MinLocalX / TexelToLocal)
			- static_cast<double>(TexelOrigin.X) - 1.0;
		const double MinRowD = FMath::FloorToDouble(MinLocalY / TexelToLocal)
			- static_cast<double>(TexelOrigin.Y) - 1.0;
		const double MaxColD = FMath::CeilToDouble(MaxLocalX / TexelToLocal)
			- static_cast<double>(TexelOrigin.X) + 1.0;
		const double MaxRowD = FMath::CeilToDouble(MaxLocalY / TexelToLocal)
			- static_cast<double>(TexelOrigin.Y) + 1.0;

		const double LastTexel = static_cast<double>(Resolution - 1);
		const int32 MinCol = static_cast<int32>(FMath::Clamp(MinColD, 0.0, static_cast<double>(Resolution)));
		const int32 MinRow = static_cast<int32>(FMath::Clamp(MinRowD, 0.0, static_cast<double>(Resolution)));
		const int32 MaxCol = static_cast<int32>(FMath::Clamp(MaxColD, -1.0, LastTexel));
		const int32 MaxRow = static_cast<int32>(FMath::Clamp(MaxRowD, -1.0, LastTexel));
		if (MinCol > MaxCol || MinRow > MaxRow)
		{
			return false;
		}
		OutRect = FIntRect(MinCol, MinRow, MaxCol + 1, MaxRow + 1);
		return true;
	}

	/** One mask's contributors folded over a raster: Resolution^2 samples spaced TexelToLocal apart from
	 *  global index TexelOrigin. Returns false when no contributor covered a single texel. */
	bool AccumulateMask(const FMaskWork& Mask, FIntPoint TexelOrigin, double TexelToLocal, int32 Resolution,
		TArray<float>& OutAccum, TBitArray<>& OutWritten, FIntRect& OutCoverage, int64& OutRectAreaSum)
	{
		const int32 TexelCount = Resolution * Resolution;
		OutAccum.Reset();
		OutAccum.SetNumZeroed(TexelCount);
		OutWritten.Init(false, TexelCount);
		OutRectAreaSum = 0;

		int32 MinCol = Resolution;
		int32 MinRow = Resolution;
		int32 MaxCol = -1;
		int32 MaxRow = -1;

		// Contributor-outer holds the fold order: a texel's state depends only on the contributors before
		// this one, and each of those already finished its whole rect.
		for (const FContributor& Contributor : Mask.Contributors)
		{
			FIntRect Rect;
			if (!ComputeContributorTexelRect(Contributor, TexelOrigin, TexelToLocal, Resolution, Rect))
			{
				continue;
			}
			OutRectAreaSum += static_cast<int64>(Rect.Width()) * static_cast<int64>(Rect.Height());

			const float HalfX = static_cast<float>(Contributor.CoverageXY.X * 0.5);
			const float HalfY = static_cast<float>(Contributor.CoverageXY.Y * 0.5);
			const float CoverageX = FMath::Max(static_cast<float>(Contributor.CoverageXY.X), 1.0e-4f);
			const float CoverageY = FMath::Max(static_cast<float>(Contributor.CoverageXY.Y), 1.0e-4f);
			// The composite's own transform (ArcTerrainComposite.usf ComputeStamp): the linear block of
			// the stamp-to-terrain inverse, which at the tile's z = 0 slab is exactly this affine
			// transform, so one texel of X is a constant patch-space step.
			const FVector StepX = Contributor.LocalToPatch.TransformVector(FVector(TexelToLocal, 0.0, 0.0));
			const double RowLocalX = static_cast<double>(TexelOrigin.X + Rect.Min.X) * TexelToLocal;

			for (int32 Row = Rect.Min.Y; Row < Rect.Max.Y; ++Row)
			{
				const double LocalY = static_cast<double>(TexelOrigin.Y + Row) * TexelToLocal;
				const FVector RowStart =
					Contributor.LocalToPatch.TransformPosition(FVector(RowLocalX, LocalY, 0.0));
				const int32 RowBase = Row * Resolution;

				for (int32 Col = Rect.Min.X; Col < Rect.Max.X; ++Col)
				{
					const double Offset = static_cast<double>(Col - Rect.Min.X);
					const float PatchX = static_cast<float>(RowStart.X + StepX.X * Offset);
					const float PatchY = static_cast<float>(RowStart.Y + StepX.Y * Offset);
					if (FMath::Abs(PatchX) > HalfX || FMath::Abs(PatchY) > HalfY)
					{
						continue;
					}

					const float MaskValue = ArcTerrainStampMask::Evaluate(
						Contributor.Mask, PatchX, PatchY, HalfX, HalfY);
					if (MaskValue <= 0.0f)
					{
						continue;
					}

					const FVector2f FootprintUV(PatchX / CoverageX + 0.5f, PatchY / CoverageY + 0.5f);
					const FVector2f SampleUV = FootprintUV * Contributor.UVScale + Contributor.UVBias;
					const float Texel = SampleBilinearClamped(*Contributor.Source, SampleUV);
					const float Value = (Texel - Contributor.ZeroInEncoding) * Contributor.EncodingScale;
					if (!FMath::IsFinite(Value))
					{
						continue;
					}

					const int32 Index = RowBase + Col;
					OutAccum[Index] = FoldValue(Contributor, OutWritten[Index], OutAccum[Index], Value, MaskValue);
					OutWritten[Index] = true;

					MinCol = FMath::Min(MinCol, Col);
					MinRow = FMath::Min(MinRow, Row);
					MaxCol = FMath::Max(MaxCol, Col);
					MaxRow = FMath::Max(MaxRow, Row);
				}
			}
		}

		if (MaxCol < MinCol)
		{
			return false;
		}
		OutCoverage = FIntRect(MinCol, MinRow, MaxCol + 1, MaxRow + 1);
		return true;
	}

	// With one contributor each, the fold reads only this geometry and the mask's own source, so masks
	// agreeing here cannot observe one another and share every pre-sample term.
	bool SharesStampGeometry(const FContributor& A, const FContributor& B)
	{
		return A.GeometryIdentity == B.GeometryIdentity
			&& A.LocalToPatch == B.LocalToPatch
			&& A.CoverageXY == B.CoverageXY
			&& A.Mask.EdgeFalloffBand == B.Mask.EdgeFalloffBand
			&& A.Mask.EdgeSharpness == B.Mask.EdgeSharpness
			&& A.Mask.CornerRadius == B.Mask.CornerRadius
			&& A.Mask.bCircleMask == B.Mask.bCircleMask
			&& A.Mask.CircleCenterOffset == B.Mask.CircleCenterOffset
			&& A.Mask.CircleRadius == B.Mask.CircleRadius
			&& A.Mask.CircleFalloffBand == B.Mask.CircleFalloffBand;
	}

	struct FFusedTarget
	{
		int32 WorkIndex = INDEX_NONE;
		const FContributor* Contributor = nullptr;

		uint8* Texels = nullptr;

		float WindowMin = 0.0f;
		float WindowMax = 0.0f;
		float Range = 0.0f;

		int32 MinCol = MAX_int32;
		int32 MinRow = MAX_int32;
		int32 MaxCol = -1;
		int32 MaxRow = -1;

		int32 CoveredTexels = 0;
		bool bSaturated = false;
	};

	void RasterFusedGroup(const FIntRect& Rect, FIntPoint TexelOrigin, double TexelToLocal,
		int32 Resolution, TArray<FFusedTarget>& Targets)
	{
		const FContributor& Geometry = *Targets[0].Contributor;
		const float HalfX = static_cast<float>(Geometry.CoverageXY.X * 0.5);
		const float HalfY = static_cast<float>(Geometry.CoverageXY.Y * 0.5);
		const float CoverageX = FMath::Max(static_cast<float>(Geometry.CoverageXY.X), 1.0e-4f);
		const float CoverageY = FMath::Max(static_cast<float>(Geometry.CoverageXY.Y), 1.0e-4f);
		const FVector StepX = Geometry.LocalToPatch.TransformVector(FVector(TexelToLocal, 0.0, 0.0));
		const double RowLocalX = static_cast<double>(TexelOrigin.X + Rect.Min.X) * TexelToLocal;
		const int32 NumTargets = Targets.Num();

		for (int32 Row = Rect.Min.Y; Row < Rect.Max.Y; ++Row)
		{
			const double LocalY = static_cast<double>(TexelOrigin.Y + Row) * TexelToLocal;
			const FVector RowStart =
				Geometry.LocalToPatch.TransformPosition(FVector(RowLocalX, LocalY, 0.0));
			const int32 RowBase = Row * Resolution;

			for (int32 Col = Rect.Min.X; Col < Rect.Max.X; ++Col)
			{
				const double Offset = static_cast<double>(Col - Rect.Min.X);
				const float PatchX = static_cast<float>(RowStart.X + StepX.X * Offset);
				const float PatchY = static_cast<float>(RowStart.Y + StepX.Y * Offset);
				if (FMath::Abs(PatchX) > HalfX || FMath::Abs(PatchY) > HalfY)
				{
					continue;
				}

				const float MaskValue = ArcTerrainStampMask::Evaluate(
					Geometry.Mask, PatchX, PatchY, HalfX, HalfY);
				if (MaskValue <= 0.0f)
				{
					continue;
				}

				const FVector2f FootprintUV(PatchX / CoverageX + 0.5f, PatchY / CoverageY + 0.5f);
				const int32 Index = RowBase + Col;

				for (int32 TargetIndex = 0; TargetIndex < NumTargets; ++TargetIndex)
				{
					FFusedTarget& Target = Targets[TargetIndex];
					const FContributor& Contributor = *Target.Contributor;

					const FVector2f SampleUV = FootprintUV * Contributor.UVScale + Contributor.UVBias;
					const float Texel = SampleBilinearClamped(*Contributor.Source, SampleUV);
					const float Value = (Texel - Contributor.ZeroInEncoding) * Contributor.EncodingScale;
					if (!FMath::IsFinite(Value))
					{
						continue;
					}

					const float Folded = FoldValue(Contributor, false, 0.0f, Value, MaskValue);
					if (Target.Texels != nullptr)
					{
						const float Normalized = (Folded - Target.WindowMin) / Target.Range;
						Target.bSaturated |= Normalized < -SaturationTolerance
							|| Normalized > 1.0f + SaturationTolerance;
						Target.Texels[Index] = static_cast<uint8>(
							FMath::Clamp(FMath::RoundHalfFromZero(Normalized * 255.0f), 0.0f, 255.0f));
					}

					++Target.CoveredTexels;
					Target.MinCol = FMath::Min(Target.MinCol, Col);
					Target.MinRow = FMath::Min(Target.MinRow, Row);
					Target.MaxCol = FMath::Max(Target.MaxCol, Col);
					Target.MaxRow = FMath::Max(Target.MaxRow, Row);
				}
			}
		}
	}

	void ResolveWindow(TConstArrayView<ArcTerrainDataMaps::FArcTerrainDataMapWindow> Windows,
		EArcTerrainDataMap Map, float& OutMin, float& OutMax)
	{
		const ArcTerrainDataMaps::FArcTerrainDataMapWindow* Window = Windows.FindByPredicate(
			[Map](const ArcTerrainDataMaps::FArcTerrainDataMapWindow& Candidate)
			{
				return Candidate.Map == Map;
			});
		OutMin = Window != nullptr ? Window->Min : 0.0f;
		OutMax = Window != nullptr ? Window->Max : 0.0f;
	}

	uint8 QuantizeZeroByte(float WindowMin, float Range)
	{
		return static_cast<uint8>(
			FMath::Clamp(FMath::RoundHalfFromZero(-WindowMin / Range * 255.0f), 0.0f, 255.0f));
	}

	void CollectSaturated(TArray<EArcTerrainDataMap>& OutMasks, EArcTerrainDataMap Map)
	{
		if (!OutMasks.Contains(Map))
		{
			OutMasks.Add(Map);
		}
	}

}

ArcTerrainDataMaps::FArcTerrainDataMapQuadResolve ArcTerrainDataMaps::ResolveMaskQuads(
	const FArcTerrainConfig& Config, double BestTexelsPerCm)
{
	using namespace ArcTerrainDataMapBake_5c1e;

	// Non-finite and zero densities are dropped BEFORE the clamp: FMath::Clamp of a NaN returns the
	// upper bound, which would let a zero-scale stamp claim the maximum.
	FArcTerrainDataMapQuadResolve Resolve;
	Resolve.UncappedQuads = FMath::Max(ArcTerrainSubsystemPrivate::CompositeQuads(Config), 1);
	if (FMath::IsFinite(BestTexelsPerCm) && BestTexelsPerCm > 0.0)
	{
		const double Resolved = FMath::RoundToDouble(BestTexelsPerCm * Config.CellSize());
		if (FMath::IsFinite(Resolved))
		{
			Resolve.UncappedQuads =
				static_cast<int32>(FMath::Clamp(Resolved, 1.0, static_cast<double>(MaxMaskQuads)));
		}
	}

	Resolve.MaxBytes = FMath::Max(
		static_cast<int64>(CVarArcTerrainDataMapMaxBytesPerMaskPerCell.GetValueOnGameThread()),
		static_cast<int64>(16));
	const int32 MaxQuads = FMath::Max(
		FMath::FloorToInt32(FMath::Sqrt(static_cast<double>(Resolve.MaxBytes))) - 3, 1);
	Resolve.Quads = FMath::Min(Resolve.UncappedQuads, MaxQuads);
	return Resolve;
}

bool ArcTerrainDataMaps::IsBakeStatsEnabled()
{
	return CVarArcTerrainDataMapBakeStats.GetValueOnGameThread() != 0;
}

void ArcTerrainDataMaps::GatherMaskWindows(const FArcTerrainState& State,
	TArray<FArcTerrainDataMapWindow>& OutWindows)
{
	using namespace ArcTerrainDataMapBake_5c1e;

	check(IsInGameThread());

	OutWindows.Reset();

	for (const FArcTerrainStampEntry& StampEntry : State.Stamps)
	{
		const UArcTerrainStampComponent* Stamp = StampEntry.Stamp.Get();
		if (Stamp == nullptr || Stamp->bIsDisabled)
		{
			continue;
		}
		const UArcTerrainTexturePatchModifier* Modifier = Cast<UArcTerrainTexturePatchModifier>(Stamp);
		if (Modifier == nullptr)
		{
			continue;
		}

		for (const TPair<EArcTerrainDataMap, const FArcTerrainDataMapSource*>& Entry : Modifier->GetDataMapSources())
		{
			const FArcTerrainDataMapSource& Source = *Entry.Value;
			if (Source.Texture == nullptr)
			{
				continue;
			}

			const float AtZero = (0.0f - Source.ZeroInEncoding) * Source.EncodingScale;
			const float AtOne = (1.0f - Source.ZeroInEncoding) * Source.EncodingScale;
			if (!FMath::IsFinite(AtZero) || !FMath::IsFinite(AtOne))
			{
				// A non-finite bound would widen the window to infinity and quantize every mask of this
				// type to a single byte; the contributor's own values are dropped by the fold anyway.
				continue;
			}

			FArcTerrainDataMapWindow& Window = FindOrAddWindow(OutWindows, Entry.Key);
			Window.Min = FMath::Min3(Window.Min, AtZero, AtOne);
			Window.Max = FMath::Max3(Window.Max, AtZero, AtOne);
		}
	}

	OutWindows.Sort([](const FArcTerrainDataMapWindow& A, const FArcTerrainDataMapWindow& B)
	{
		return static_cast<uint8>(A.Map) < static_cast<uint8>(B.Map);
	});
}

bool ArcTerrainDataMaps::RefreshDataMaskWindows(FArcTerrainState& State, double& OutGatherSeconds)
{
	using namespace ArcTerrainDataMapBake_5c1e;

	TArray<FArcTerrainDataMapWindow> NewWindows;
	const double StartSeconds = FPlatformTime::Seconds();
	GatherMaskWindows(State, NewWindows);
	OutGatherSeconds = FPlatformTime::Seconds() - StartSeconds;
	const FIoHash NewFingerprint = ComputeMaskWindowFingerprint(NewWindows);
	if (NewFingerprint == State.DataMaskWindowFingerprint)
	{
		return false;
	}
	State.DataMaskWindows = MoveTemp(NewWindows);
	State.DataMaskWindowFingerprint = NewFingerprint;
	return true;
}

TConstArrayView<ArcTerrainDataMaps::FArcTerrainDataMapWindow>
ArcTerrainDataMaps::GetOrGatherMaskWindows(FArcTerrainState& State, double& OutGatherSeconds)
{
	OutGatherSeconds = 0.0;
	if (State.DataMaskWindowFingerprint.IsZero() && State.DataWindowRevision == 0)
	{
		RefreshDataMaskWindows(State, OutGatherSeconds);
	}
	return State.DataMaskWindows;
}

void ArcTerrainDataMaps::GatherDataMapTileSnapshot(
	const FTransform& TerrainTransform,
	FIntPoint TileKey,
	TConstArrayView<UArcTerrainStampComponent* const> SortedStamps,
	TConstArrayView<FArcTerrainDataMapWindow> MaskWindows,
	uint32 DataWindowRevision,
	double MaskWindowGatherSeconds,
	int32 TileQuads,
	double TexelToLocal,
	FArcTerrainDataMapBakeScope& Scope,
	FArcTerrainDataMapTileSnapshot& OutSnapshot)
{
	using namespace ArcTerrainDataMapBake_5c1e;

	check(IsInGameThread());
	OutSnapshot = FArcTerrainDataMapTileSnapshot();
	OutSnapshot.TileKey = TileKey;
	OutSnapshot.TileQuads = TileQuads;
	OutSnapshot.TexelToLocal = TexelToLocal;
	OutSnapshot.DataWindowRevision = DataWindowRevision;
	OutSnapshot.MaskWindows.Append(MaskWindows.GetData(), MaskWindows.Num());
	OutSnapshot.bBakeStats = IsBakeStatsEnabled();
	OutSnapshot.MaskWindowGatherSeconds = MaskWindowGatherSeconds;
	const double GatherStartSeconds = OutSnapshot.bBakeStats ? FPlatformTime::Seconds() : 0.0;
	GatherMaskWork(TerrainTransform, SortedStamps, Scope, OutSnapshot.Work);
	OutSnapshot.GatherSeconds = OutSnapshot.bBakeStats
		? FPlatformTime::Seconds() - GatherStartSeconds : 0.0;
}

FArcTerrainDataMapTileBakeResult ArcTerrainDataMaps::BakeDataMapTileSnapshot(
	const FArcTerrainDataMapTileSnapshot& Snapshot)
{
	using namespace ArcTerrainDataMapBake_5c1e;

	FArcTerrainDataMapTileBakeResult Result;
	Result.DataWindowRevision = Snapshot.DataWindowRevision;
	const double WorkerStartSeconds = Snapshot.bBakeStats ? FPlatformTime::Seconds() : 0.0;
	FArcTerrainDataMapCellBake& OutBake = Result.Bake;
	OutBake.Key = Snapshot.TileKey;
	if (Snapshot.TileQuads < 1 || Snapshot.TexelToLocal <= 0.0)
	{
		return Result;
	}

	const int32 TileQuads = Snapshot.TileQuads;
	const double TexelToLocal = Snapshot.TexelToLocal;
	const FIntPoint TileKey = Snapshot.TileKey;
	const TArray<FMaskWork>& Work = Snapshot.Work;
	const TArray<FArcTerrainDataMapWindow>& MaskWindows = Snapshot.MaskWindows;
	const bool bStats = Snapshot.bBakeStats;
	TArray<FMaskStat>& MaskStats = Result.MaskStats;
	double AccumSeconds = 0.0;
	double QuantSeconds = 0.0;
	double StatsSeconds = 0.0;

	const int32 Resolution = TileQuads + 3;
	const int32 TexelCount = Resolution * Resolution;
	Result.bBakeStats = bStats;
	Result.Resolution = Resolution;
	Result.TexelCount = TexelCount;
	Result.GatherSeconds = Snapshot.GatherSeconds;
	Result.MaskWindowGatherSeconds = Snapshot.MaskWindowGatherSeconds;
	const FIntPoint TexelOrigin =
		ArcTerrainSubsystemPrivate::ComputeGlobalTexelOrigin(TileKey, TileQuads);

	OutBake.Masks.SetNum(Work.Num());
	if (bStats)
	{
		MaskStats.SetNum(Work.Num());
	}
	for (int32 WorkIndex = 0; WorkIndex < Work.Num(); ++WorkIndex)
	{
		OutBake.Masks[WorkIndex].Map = Work[WorkIndex].Map;
		OutBake.Masks[WorkIndex].bUnresolved = Work[WorkIndex].bUnresolved;
		if (bStats)
		{
			MaskStats[WorkIndex].Map = Work[WorkIndex].Map;
			MaskStats[WorkIndex].Contributors = Work[WorkIndex].Contributors.Num();
		}
	}

	TArray<TArray<int32>> FusedGroups;
	for (int32 WorkIndex = 0; WorkIndex < Work.Num(); ++WorkIndex)
	{
		if (Work[WorkIndex].Contributors.Num() != 1)
		{
			continue;
		}
		int32 GroupIndex = INDEX_NONE;
		for (int32 Candidate = 0; Candidate < FusedGroups.Num(); ++Candidate)
		{
			if (SharesStampGeometry(Work[FusedGroups[Candidate][0]].Contributors[0],
				Work[WorkIndex].Contributors[0]))
			{
				GroupIndex = Candidate;
				break;
			}
		}
		if (GroupIndex == INDEX_NONE)
		{
			GroupIndex = FusedGroups.AddDefaulted();
		}
		FusedGroups[GroupIndex].Add(WorkIndex);
	}

	TArray<FFusedTarget> Targets;
	for (const TArray<int32>& Group : FusedGroups)
	{
		const double FusedStartSeconds = bStats ? FPlatformTime::Seconds() : 0.0;

		FIntRect Rect;
		const bool bHasRect = ComputeContributorTexelRect(
			Work[Group[0]].Contributors[0], TexelOrigin, TexelToLocal, Resolution, Rect);

		Targets.Reset();
		Targets.Reserve(Group.Num());
		for (int32 WorkIndex : Group)
		{
			FFusedTarget& Target = Targets.AddDefaulted_GetRef();
			Target.WorkIndex = WorkIndex;
			Target.Contributor = &Work[WorkIndex].Contributors[0];
			ResolveWindow(MaskWindows, Work[WorkIndex].Map, Target.WindowMin, Target.WindowMax);
			Target.Range = Target.WindowMax - Target.WindowMin;
			if (bHasRect && Target.Range > UE_KINDA_SMALL_NUMBER)
			{
				FArcTerrainDataMapCellMask& Out = OutBake.Masks[WorkIndex];
				Out.Texels.Init(QuantizeZeroByte(Target.WindowMin, Target.Range), TexelCount);
				Target.Texels = Out.Texels.GetData();
			}
		}

		if (bHasRect)
		{
			RasterFusedGroup(Rect, TexelOrigin, TexelToLocal, Resolution, Targets);
		}

		for (const FFusedTarget& Target : Targets)
		{
			FArcTerrainDataMapCellMask& Out = OutBake.Masks[Target.WorkIndex];
			if (Target.MaxCol < Target.MinCol)
			{
				Out.Texels.Empty();
				continue;
			}
			Out.Quads = TileQuads;
			Out.CoverageRect =
				FIntRect(Target.MinCol, Target.MinRow, Target.MaxCol + 1, Target.MaxRow + 1);
			Out.QuantMin = Target.WindowMin;
			Out.QuantMax = Target.Range <= UE_KINDA_SMALL_NUMBER ? Target.WindowMin : Target.WindowMax;
			if (Target.Texels == nullptr)
			{
				Out.Texels.SetNumZeroed(TexelCount);
			}
		}

		if (bStats)
		{
			const double FusedEndSeconds = FPlatformTime::Seconds();
			const double ElapsedSeconds = FusedEndSeconds - FusedStartSeconds;
			AccumSeconds += ElapsedSeconds;
			const int64 RectArea = bHasRect
				? static_cast<int64>(Rect.Width()) * static_cast<int64>(Rect.Height()) : 0;
			for (int32 TargetIndex = 0; TargetIndex < Targets.Num(); ++TargetIndex)
			{
				FMaskStat& Stat = MaskStats[Targets[TargetIndex].WorkIndex];
				Stat.FusedMasks = Targets.Num();
				Stat.RectAreaSum = RectArea;
				Stat.CoveredTexels = Targets[TargetIndex].CoveredTexels;
				Stat.AccumMs = TargetIndex == 0 ? ElapsedSeconds * 1000.0 : 0.0;
			}
			StatsSeconds += FPlatformTime::Seconds() - FusedEndSeconds;
		}

		for (const FFusedTarget& Target : Targets)
		{
			if (Target.bSaturated)
			{
				CollectSaturated(Result.SaturatedMasks, Work[Target.WorkIndex].Map);
			}
		}
	}

	TArray<float> Accum;
	TBitArray<> Written;

	for (int32 WorkIndex = 0; WorkIndex < Work.Num(); ++WorkIndex)
	{
		const FMaskWork& Mask = Work[WorkIndex];
		if (Mask.Contributors.Num() < 2)
		{
			continue;
		}

		FArcTerrainDataMapCellMask& Out = OutBake.Masks[WorkIndex];

		FIntRect Coverage;
		int64 RectAreaSum = 0;
		const double AccumStartSeconds = bStats ? FPlatformTime::Seconds() : 0.0;
		const bool bAccumulated = AccumulateMask(
			Mask, TexelOrigin, TexelToLocal, Resolution, Accum, Written, Coverage, RectAreaSum);
		const double AccumEndSeconds = bStats ? FPlatformTime::Seconds() : 0.0;
		if (bStats)
		{
			const double ElapsedSeconds = AccumEndSeconds - AccumStartSeconds;
			AccumSeconds += ElapsedSeconds;
			MaskStats[WorkIndex].AccumMs = ElapsedSeconds * 1000.0;
			MaskStats[WorkIndex].RectAreaSum = RectAreaSum;
			MaskStats[WorkIndex].CoveredTexels = Written.CountSetBits();
			StatsSeconds += FPlatformTime::Seconds() - AccumEndSeconds;
		}
		if (!bAccumulated)
		{
			continue;
		}

		// The window travels per plane in the tile header: FlowMapX/Y decode to [-1, 1] and
		// Deposits to centimetres of soil, so a fixed [0, 1] encoding clips both.
		float WindowMin = 0.0f;
		float WindowMax = 0.0f;
		ResolveWindow(MaskWindows, Mask.Map, WindowMin, WindowMax);
		const float Range = WindowMax - WindowMin;

		const double QuantStartSeconds = bStats ? FPlatformTime::Seconds() : 0.0;
		Out.Quads = TileQuads;
		Out.CoverageRect = Coverage;
		Out.QuantMin = WindowMin;
		Out.QuantMax = Range <= UE_KINDA_SMALL_NUMBER ? WindowMin : WindowMax;
		if (Range <= UE_KINDA_SMALL_NUMBER)
		{
			Out.Texels.SetNumZeroed(TexelCount);
			if (bStats)
			{
				const double ElapsedSeconds = FPlatformTime::Seconds() - QuantStartSeconds;
				QuantSeconds += ElapsedSeconds;
				MaskStats[WorkIndex].QuantMs = ElapsedSeconds * 1000.0;
			}
			continue;
		}

		Out.Texels.Init(QuantizeZeroByte(WindowMin, Range), TexelCount);
		bool bSaturated = false;
		for (int32 Row = Coverage.Min.Y; Row < Coverage.Max.Y; ++Row)
		{
			const int32 RowBase = Row * Resolution;
			for (int32 Col = Coverage.Min.X; Col < Coverage.Max.X; ++Col)
			{
				const int32 Index = RowBase + Col;
				if (!Written[Index])
				{
					continue;
				}
				const float Normalized = (Accum[Index] - WindowMin) / Range;
				bSaturated |= Normalized < -SaturationTolerance
					|| Normalized > 1.0f + SaturationTolerance;
				Out.Texels[Index] = static_cast<uint8>(
					FMath::Clamp(FMath::RoundHalfFromZero(Normalized * 255.0f), 0.0f, 255.0f));
			}
		}
		if (bStats)
		{
			const double ElapsedSeconds = FPlatformTime::Seconds() - QuantStartSeconds;
			QuantSeconds += ElapsedSeconds;
			MaskStats[WorkIndex].QuantMs = ElapsedSeconds * 1000.0;
		}

		if (bSaturated)
		{
			CollectSaturated(Result.SaturatedMasks, Mask.Map);
		}
	}

	if (bStats)
	{
		Result.AccumSeconds = AccumSeconds;
		Result.QuantSeconds = QuantSeconds;
		Result.StatsSeconds = StatsSeconds;
		Result.WorkerSeconds = FPlatformTime::Seconds() - WorkerStartSeconds;
	}
	return Result;
}

void ArcTerrainDataMaps::ReportDataMapTileBakeWarnings(const FArcTerrainDataMapTileBakeResult& Result,
	TConstArrayView<FArcTerrainDataMapWindow> MaskWindows, FArcTerrainDataMapBakeScope& Scope)
{
	using namespace ArcTerrainDataMapBake_5c1e;

	check(IsInGameThread());
	for (EArcTerrainDataMap Map : Result.SaturatedMasks)
	{
		if (Scope.WarnedSaturatedMasks.Contains(static_cast<uint8>(Map)))
		{
			continue;
		}
		Scope.WarnedSaturatedMasks.Add(static_cast<uint8>(Map));
		float WindowMin = 0.0f;
		float WindowMax = 0.0f;
		ResolveWindow(MaskWindows, Map, WindowMin, WindowMax);
		UE_LOGF(LogArcTerrain, Warning,
			"ArcTerrain data map '%ls': folded values fall outside the terrain-wide quantization "
			"window [%f, %f] and are clamped to it. The authored decode bounds one contributor, so "
			"either additive contributors stack past it or a floating-point source carries texels "
			"outside [0, 1].",
			*MaskName(Map), WindowMin, WindowMax);
	}
}

void ArcTerrainDataMaps::ReportDataMapTileBakeStats(const FArcTerrainDataMapTileBakeResult& Result)
{
	using namespace ArcTerrainDataMapBake_5c1e;

	check(IsInGameThread());
	if (!Result.bBakeStats)
	{
		return;
	}
	const double TotalSeconds = Result.GatherSeconds + Result.MaskWindowGatherSeconds + Result.WorkerSeconds;
	const double OtherSeconds = Result.WorkerSeconds - Result.AccumSeconds - Result.QuantSeconds
		- Result.StatsSeconds;
	const FString MaskText = FormatMaskStats(Result.MaskStats, Result.TexelCount);
	UE_LOGF(LogArcTerrain, Log,
		"ArcTerrainBakeStats tile=%d,%d res=%d texels=%d nmasks=%d gather_ms=%.3f "
		"windows_ms=%.3f accum_ms=%.3f quant_ms=%.3f stats_ms=%.3f other_ms=%.3f "
		"total_ms=%.3f masks=[%ls]",
		Result.Bake.Key.X, Result.Bake.Key.Y, Result.Resolution, Result.TexelCount, Result.MaskStats.Num(),
		Result.GatherSeconds * 1000.0, Result.MaskWindowGatherSeconds * 1000.0,
		Result.AccumSeconds * 1000.0, Result.QuantSeconds * 1000.0, Result.StatsSeconds * 1000.0,
		OtherSeconds * 1000.0, TotalSeconds * 1000.0, *MaskText);
}

int64 ArcTerrainDataMaps::SnapshotSourceBytes(const FArcTerrainDataMapTileSnapshot& Snapshot)
{
	TSet<const FArcTerrainSourceCpuFloats*> SeenSources;
	int64 Result = 0;
	for (const FArcTerrainDataMapTileSnapshot::FMaskWork& Mask : Snapshot.Work)
	{
		for (const FArcTerrainDataMapTileSnapshot::FContributor& Contributor : Mask.Contributors)
		{
			const FArcTerrainSourceCpuFloats* Source = Contributor.Source.Get();
			bool bAlreadySeen = false;
			if (Source != nullptr)
			{
				SeenSources.Add(Source, &bAlreadySeen);
			}
			if (Source != nullptr && !bAlreadySeen)
			{
				Result += Source->SizeBytes;
			}
		}
	}
	return Result;
}

#endif // WITH_EDITOR
