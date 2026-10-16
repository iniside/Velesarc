// Copyright Lukasz Baran. All Rights Reserved.

#include "FoliageSkinning/ArcFoliageMaskTracer.h"

#include "ImageCore.h"
#include "IntVectorTypes.h"

#include "Engine/Texture.h"
#include "Engine/Texture2D.h"

#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameters.h"

#include "SpatialAlgo/PCGMarchingSquares.h"

// Unique, file-scoped implementation namespace (project rule: cpp free functions in a unique namespace).
namespace ArcFoliageMaskTracer_Impl
{
	using FVector2i = UE::Geometry::FVector2i;
	using FPolygon2f = UE::Geometry::FPolygon2f;

	static int32 GetPixelIndex(const FVector2i& PixelCoords, int32 ImageWidth)
	{
		return PixelCoords.X + PixelCoords.Y * ImageWidth;
	}

	static FVector2f PixelCoordToUV(const FVector2i& PixelCoord, const FVector2i& ImageSize)
	{
		const float U = ImageSize.X > 1 ? PixelCoord.X / static_cast<float>(ImageSize.X - 1) : 0.0f;
		const float V = ImageSize.Y > 1 ? PixelCoord.Y / static_cast<float>(ImageSize.Y - 1) : 0.0f;
		return FVector2f(U, V);
	}

	/**
	 * Rasterize @p InTexture2D's chosen channel into a black/white grid of size @p InOutSampleSize
	 * (clamped down to the source mip-0 size). A texel is "white" (opaque) when its channel value,
	 * optionally inverted, exceeds @p WhiteLevel.
	 * Editor-only: FTextureSource::FMipLock is not available in non-editor builds.
	 */
	static float ChannelValue(const FLinearColor& PixelColor, EArcMaskChannel ColorChannel)
	{
		switch (ColorChannel)
		{
		case EArcMaskChannel::Red:
			return PixelColor.R;
		case EArcMaskChannel::Green:
			return PixelColor.G;
		case EArcMaskChannel::Blue:
			return PixelColor.B;
		case EArcMaskChannel::Alpha:
			return PixelColor.A;
		default:
			return 0.0f;
		}
	}

#if WITH_EDITOR
	/**
	 * Resolve EArcMaskChannel::Auto to the concrete channel that actually carries the mask. A foliage mask
	 * lives in whichever channel has the widest spread of values: a standalone grayscale translucency map
	 * puts it in RGB (alpha is flat 1.0), a diffuse-with-packed-mask puts it in Alpha. We pick the channel
	 * with the highest variance over a coarse grid; a near-constant channel (variance ~0) is never the mask.
	 */
	static EArcMaskChannel PickBestChannel(const FImageView& Image)
	{
		const int32 ImageWidth = Image.GetWidth();
		const int32 ImageHeight = Image.GetHeight();
		const int32 SamplesPerAxis = FMath::Min(64, FMath::Min(ImageWidth, ImageHeight));
		if (SamplesPerAxis < 2)
		{
			return EArcMaskChannel::Red;
		}

		const EArcMaskChannel Candidates[4] = { EArcMaskChannel::Red, EArcMaskChannel::Green, EArcMaskChannel::Blue, EArcMaskChannel::Alpha };
		double Sum[4] = { 0.0, 0.0, 0.0, 0.0 };
		double SumSq[4] = { 0.0, 0.0, 0.0, 0.0 };
		int32 Count = 0;

		for (int32 sx = 0; sx < SamplesPerAxis; sx++)
		{
			for (int32 sy = 0; sy < SamplesPerAxis; sy++)
			{
				const int32 SamplePosX = FMath::RoundToInt((sx / static_cast<float>(SamplesPerAxis - 1)) * (ImageWidth - 1));
				const int32 SamplePosY = FMath::RoundToInt((sy / static_cast<float>(SamplesPerAxis - 1)) * (ImageHeight - 1));
				const FLinearColor PixelColor = Image.GetOnePixelLinear(SamplePosX, SamplePosY);
				for (int32 c = 0; c < 4; c++)
				{
					const double Value = ChannelValue(PixelColor, Candidates[c]);
					Sum[c] += Value;
					SumSq[c] += Value * Value;
				}
				++Count;
			}
		}

		EArcMaskChannel BestChannel = EArcMaskChannel::Red;
		double BestVariance = -1.0;
		for (int32 c = 0; c < 4; c++)
		{
			const double Mean = Sum[c] / Count;
			const double Variance = SumSq[c] / Count - Mean * Mean;
			if (Variance > BestVariance)
			{
				BestVariance = Variance;
				BestChannel = Candidates[c];
			}
		}
		return BestChannel;
	}
#endif

	static bool SampleImage(
		UTexture2D& InTexture2D,
		FVector2i& InOutSampleSize,
		TArray<bool>& OutBlackAndWhiteImageData,
		bool bInvertImage,
		float WhiteLevel,
		EArcMaskChannel ColorChannel)
	{
#if WITH_EDITOR
		FTextureSource& InTextureSource = InTexture2D.Source;
		if (!InTextureSource.IsValid())
		{
			return false;
		}

		const FTextureSource::FMipLock LockedMip0(FTextureSource::ELockState::ReadOnly, &InTextureSource, 0);
		if (!LockedMip0.IsValid())
		{
			return false;
		}

		const EArcMaskChannel ResolvedChannel = ColorChannel == EArcMaskChannel::Auto
			? PickBestChannel(LockedMip0.Image)
			: ColorChannel;

		const int32 ImageWidth = LockedMip0.Image.GetWidth();
		const int32 ImageHeight = LockedMip0.Image.GetHeight();
		InOutSampleSize.X = FMath::Min(InOutSampleSize.X, ImageWidth);
		InOutSampleSize.Y = FMath::Min(InOutSampleSize.Y, ImageHeight);

		OutBlackAndWhiteImageData.SetNum(InOutSampleSize.X * InOutSampleSize.Y);
		for (int32 x = 0; x < InOutSampleSize.X; x++)
		{
			for (int32 y = 0; y < InOutSampleSize.Y; y++)
			{
				const FVector2i PixelCoords = FVector2i(x, y);
				const FVector2f UV = PixelCoordToUV(PixelCoords, InOutSampleSize);

				const int32 SamplePosX = FMath::RoundToInt(UV.X * (ImageWidth - 1));
				const int32 SamplePosY = FMath::RoundToInt(UV.Y * (ImageHeight - 1));

				const FLinearColor PixelColor = LockedMip0.Image.GetOnePixelLinear(SamplePosX, SamplePosY);
				const float MaskedColor = ChannelValue(PixelColor, ResolvedChannel);

				const float Alpha = bInvertImage ? 1.0f - MaskedColor : MaskedColor;
				const int32 PixelIndex = GetPixelIndex(PixelCoords, InOutSampleSize.X);
				OutBlackAndWhiteImageData[PixelIndex] = Alpha > WhiteLevel;
			}
		}

		return true;
#else
		return false;
#endif
	}

	/**
	 * Trace closed contours of the opaque region with Marching Squares and normalize each to UV [0,1].
	 * V convention (kept from the PV source): V = row/(H-1), so UV (0,0) is the texture's top-left texel.
	 */
	static void TraceImage(const TArray<bool>& ImageData, const FVector2i& ImageSize, TArray<FPolygon2f>& OutCurves)
	{
		const TFunctionRef<double(int32, int32)> ValueQuery = [&ImageData, &ImageSize](int32 X, int32 Y) -> double
		{
			return ImageData[GetPixelIndex(FVector2i(X, Y), ImageSize.X)];
		};

		const TArray<PCGSpatialAlgo::FPCGMarchingSquareResult> Results = PCGSpatialAlgo::MarchingSquares(
			ImageSize.X,
			ImageSize.Y,
			1,
			ValueQuery,
			false);

		TArray<FVector2f> PerimeterCurve;
		OutCurves.Reserve(Results.Num());
		for (const PCGSpatialAlgo::FPCGMarchingSquareResult& Result : Results)
		{
			if (!Result.bClosed)
			{
				continue;
			}

			PerimeterCurve.Reset(Result.LinkedGridCoordinates.Num());
			for (const FVector2D& Coordinate : Result.LinkedGridCoordinates)
			{
				PerimeterCurve.Add(FVector2f(Coordinate.X / (ImageSize.X - 1), Coordinate.Y / (ImageSize.Y - 1)));
			}
			OutCurves.Emplace(MoveTemp(PerimeterCurve));
		}
	}

	/** Drop curves whose UV-space bounding box area is below @p Threshold (swap-remove; O(N) per element, no shift). */
	static void RemoveSmallCurves(TArray<FPolygon2f>& InOutPerimeterCurves, float Threshold)
	{
		const float ThresholdSqr = Threshold * Threshold;
		for (int32 i = InOutPerimeterCurves.Num() - 1; i >= 0; i--)
		{
			const TArray<FVector2f>& Curve = InOutPerimeterCurves[i].GetVertices();
			FVector2f Min = FVector2f(MAX_flt);
			FVector2f Max = FVector2f(-MAX_flt);

			for (const FVector2f& Point : Curve)
			{
				Min.X = FMath::Min(Min.X, Point.X);
				Min.Y = FMath::Min(Min.Y, Point.Y);
				Max.X = FMath::Max(Max.X, Point.X);
				Max.Y = FMath::Max(Max.Y, Point.Y);
			}

			const float Area = (Max.X - Min.X) * (Max.Y - Min.Y);
			const float AreaSqr = Area * Area;
			if (AreaSqr < ThresholdSqr)
			{
				InOutPerimeterCurves.RemoveAtSwap(i);
			}
		}
	}

	/** Neighbour-average each curve vertex @p SmoothingIterations times (wrap-around, closed curve). */
	static void SmoothCurves(TArray<FPolygon2f>& InOutPerimeterCurves, int32 SmoothingIterations)
	{
		while (SmoothingIterations > 0)
		{
			for (FPolygon2f& PerimeterCurve : InOutPerimeterCurves)
			{
				const TArray<FVector2f>& Vertices = PerimeterCurve.GetVertices();

				TArray<FVector2f> SmoothedCurve;
				SmoothedCurve.SetNum(Vertices.Num());

				for (int32 i = 0; i < Vertices.Num(); i++)
				{
					const FVector2f& Pos1 = i == 0 ? Vertices.Last() : Vertices[i - 1];
					const FVector2f& Pos2 = Vertices[i];
					const FVector2f& Pos3 = i == Vertices.Num() - 1 ? Vertices[0] : Vertices[i + 1];

					const FVector2f AvgPos = (Pos1 + Pos2 + Pos3) / 3.0f;
					SmoothedCurve[i] = AvgPos;
				}

				PerimeterCurve = FPolygon2f(MoveTemp(SmoothedCurve));
			}

			SmoothingIterations -= 1;
		}
	}

	/**
	 * Simplify each curve. Mapping kept exactly as the PV source:
	 *   LineDeviationTolerance = SimplificationAmount * 0.001
	 *   ClusterTolerance       = LineDeviationTolerance * 0.001
	 * (Both are in UV units.)
	 */
	static void SimplifyCurves(TArray<FPolygon2f>& InOutPerimeterCurves, float SimplificationAmount)
	{
		for (FPolygon2f& PerimeterCurve : InOutPerimeterCurves)
		{
			const float LineDeviationTolerance = SimplificationAmount * 0.001f;
			const float ClusterTolerance = LineDeviationTolerance * 0.001f;
			PerimeterCurve.Simplify(ClusterTolerance, LineDeviationTolerance);
		}
	}

	/** Case-insensitive priority score for a texture-parameter name; higher wins, 0 = no match. */
	static int32 ScoreMaskParamName(const FString& ParamName)
	{
		if (ParamName.Contains(TEXT("Translucency"), ESearchCase::IgnoreCase))
		{
			return 4;
		}
		if (ParamName.Contains(TEXT("Opacity"), ESearchCase::IgnoreCase))
		{
			return 3;
		}
		if (ParamName.Contains(TEXT("Mask"), ESearchCase::IgnoreCase))
		{
			return 2;
		}
		if (ParamName.Contains(TEXT("Alpha"), ESearchCase::IgnoreCase))
		{
			return 1;
		}
		return 0;
	}
}

namespace Arc::FoliageMask
{
	ETraceResult TracePerimeterCurves(
		UTexture2D* Texture,
		const FArcMaskTraceParams& Params,
		TArray<UE::Geometry::FPolygon2f>& OutCurves)
	{
		using namespace ArcFoliageMaskTracer_Impl;

		OutCurves.Reset();

		if (!Texture)
		{
			return ETraceResult::InvalidSourceTexture;
		}

		// Non-uniform textures are treated as uniform, so SampleResolution is not scaled by aspect ratio.
		FVector2i SampleSize = FVector2i(Params.SampleResolution, Params.SampleResolution);
		if (SampleSize.X <= 1 || SampleSize.Y <= 1)
		{
			return ETraceResult::InvalidSampleSize;
		}

		TArray<bool> BlackAndWhiteImageData;
		if (!SampleImage(*Texture, SampleSize, BlackAndWhiteImageData, Params.bInvert, Params.WhiteLevel, Params.Channel))
		{
			return ETraceResult::InvalidSourceTexture;
		}

		// SampleImage clamps SampleSize down to the real texture size, which may collapse it.
		if (SampleSize.X <= 1 || SampleSize.Y <= 1)
		{
			return ETraceResult::InvalidTextureSize;
		}

		TraceImage(BlackAndWhiteImageData, SampleSize, OutCurves);
		RemoveSmallCurves(OutCurves, Params.MinCurveBoundsArea);
		SmoothCurves(OutCurves, Params.SmoothingIterations);
		SimplifyCurves(OutCurves, Params.SimplificationAmount);

		if (OutCurves.Num() == 0)
		{
			return ETraceResult::NoClosedCurves;
		}

		return ETraceResult::Success;
	}

	UTexture2D* ResolveMaskTexture(const UMaterialInterface* Material)
	{
		using namespace ArcFoliageMaskTracer_Impl;

		if (!Material)
		{
			return nullptr;
		}

		TArray<FMaterialParameterInfo> ParameterInfos;
		TArray<FGuid> ParameterIds;
		Material->GetAllTextureParameterInfo(ParameterInfos, ParameterIds);

		UTexture2D* BestTexture = nullptr;
		int32 BestScore = 0;
		for (const FMaterialParameterInfo& ParameterInfo : ParameterInfos)
		{
			const int32 Score = ScoreMaskParamName(ParameterInfo.Name.ToString());
			if (Score <= BestScore)
			{
				continue;
			}

			UTexture* TextureValue = nullptr;
			if (!Material->GetTextureParameterValue(ParameterInfo, TextureValue) || !TextureValue)
			{
				continue;
			}

			UTexture2D* Texture2D = Cast<UTexture2D>(TextureValue);
			if (!Texture2D)
			{
				continue;
			}

			BestTexture = Texture2D;
			BestScore = Score;
		}

		// Fallback: GetAllTextureParameterInfo walks the parent chain, which yields nothing when the
		// parent master material is missing on disk (common for imported Megascans instances). The
		// instance still stores its own override values — score those directly.
		if (!BestTexture)
		{
			const UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(Material);
			if (MaterialInstance)
			{
				for (const FTextureParameterValue& ParameterValue : MaterialInstance->TextureParameterValues)
				{
					const int32 Score = ScoreMaskParamName(ParameterValue.ParameterInfo.Name.ToString());
					if (Score <= BestScore)
					{
						continue;
					}

					UTexture2D* Texture2D = Cast<UTexture2D>(ParameterValue.ParameterValue);
					if (!Texture2D)
					{
						continue;
					}

					BestTexture = Texture2D;
					BestScore = Score;
				}
			}
		}

		return BestTexture;
	}

	float ResolveWhiteLevel(const UMaterialInterface* Material)
	{
		if (!Material)
		{
			return 0.333f;
		}
		// A clip value at (or outside) the 0/1 extremes classifies the whole texture as one region and
		// the trace finds no boundary. Materials with a missing parent master report 0.0 here — fall
		// back to the engine-default clip value instead.
		const float ClipValue = Material->GetOpacityMaskClipValue();
		if (ClipValue < 0.01f || ClipValue > 0.99f)
		{
			return 0.333f;
		}
		return ClipValue;
	}
}
