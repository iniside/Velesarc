// Copyright Lukasz Baran. All Rights Reserved.

#include "Preview/SArcRecoilPatternCanvas.h"

#include "ArcRecoilImpulse.h"
#include "ArcRecoilModule.h"
#include "Layout/SlateRect.h"
#include "Math/RandomStream.h"
#include "Preview/ArcRecoilPreviewSettings.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateLayoutTransform.h"
#include "Styling/CoreStyle.h"

namespace ArcRecoilPatternCanvasPrivate
{
	constexpr float MarginFraction = 1.15f;
	constexpr float MinHalfExtentDeg = 0.01f;
	constexpr float PlotInsetPx = 6.0f;
	constexpr int32 MaxPolylinePoints = 512;
	constexpr int32 MaxGridLines = 16;
	constexpr int32 EllipseSegments = 64;
	constexpr int32 TrailSampleCount = 24;
	constexpr int32 MaxSpreadSamples = 4096;
	constexpr uint32 VizSeedOffset = 0x5EED17u;

	const FLinearColor BackgroundColor(0.020f, 0.020f, 0.024f, 1.0f);
	const FLinearColor GridColor(0.120f, 0.125f, 0.145f, 1.0f);
	const FLinearColor AxisColor(0.320f, 0.330f, 0.380f, 1.0f);
	const FLinearColor LabelColor(0.620f, 0.640f, 0.700f, 1.0f);
	const FLinearColor CaptionColor(0.850f, 0.700f, 0.350f, 1.0f);
	const FLinearColor SeedPathColor(0.380f, 0.680f, 1.000f, 0.25f);
	const FLinearColor MeanPathColor(0.450f, 0.800f, 1.000f, 1.0f);
	const FLinearColor MarkerColor(1.000f, 0.870f, 0.250f, 1.0f);
	const FLinearColor ClampEllipseColor(1.000f, 0.350f, 0.280f, 0.85f);
	const FLinearColor CloudColor(1.000f, 0.760f, 0.240f, 1.0f);
	const FLinearColor SpreadCircleColor(0.380f, 0.950f, 0.520f, 0.70f);
	const FLinearColor ImpactPointColor(0.780f, 1.000f, 0.820f, 0.55f);
	const FLinearColor ReticleColor(1.000f, 1.000f, 1.000f, 0.90f);

	FVector2f DegToScreen(const FVector2f& Deg, const FVector2f& Centre, const float PxPerDeg)
	{
		// Screen Y is inverted: Deg.X is pitch and grows upward, Slate's Y grows downward.
		return FVector2f(Centre.X + Deg.Y * PxPerDeg, Centre.Y - Deg.X * PxPerDeg);
	}

	float NiceGridStep(const float HalfExtentDeg)
	{
		const float Raw = FMath::Max(HalfExtentDeg, MinHalfExtentDeg) / 3.0f;
		const float Magnitude = FMath::Pow(10.0f, FMath::FloorToFloat(FMath::LogX(10.0f, Raw)));
		if (!(Magnitude > 0.0f) || !FMath::IsFinite(Magnitude))
		{
			return HalfExtentDeg;
		}

		const float Normalized = Raw / Magnitude;
		float Multiple = 1.0f;
		if (Normalized > 5.0f)
		{
			Multiple = 10.0f;
		}
		else if (Normalized > 2.0f)
		{
			Multiple = 5.0f;
		}
		else if (Normalized > 1.0f)
		{
			Multiple = 2.0f;
		}

		return Multiple * Magnitude;
	}

	FString FormatDegrees(const float Value, const float Step)
	{
		if (Step >= 1.0f)
		{
			return FString::Printf(TEXT("%.0f"), Value);
		}
		if (Step >= 0.1f)
		{
			return FString::Printf(TEXT("%.1f"), Value);
		}
		return FString::Printf(TEXT("%.2f"), Value);
	}

	bool IsPointVisible(const FVector2f& Point, const FSlateRect& CullRect, const float Radius)
	{
		return Point.X >= CullRect.Left - Radius && Point.X <= CullRect.Right + Radius
			&& Point.Y >= CullRect.Top - Radius && Point.Y <= CullRect.Bottom + Radius;
	}

	bool IsPolylineVisible(const TArray<FVector2f>& Points, const FSlateRect& CullRect)
	{
		if (Points.Num() < 2)
		{
			return false;
		}

		FVector2f BoxMin = Points[0];
		FVector2f BoxMax = Points[0];
		for (const FVector2f& Point : Points)
		{
			BoxMin = FVector2f(FMath::Min(BoxMin.X, Point.X), FMath::Min(BoxMin.Y, Point.Y));
			BoxMax = FVector2f(FMath::Max(BoxMax.X, Point.X), FMath::Max(BoxMax.Y, Point.Y));
		}

		return FSlateRect::DoRectanglesIntersect(FSlateRect(BoxMin.X, BoxMin.Y, BoxMax.X, BoxMax.Y), CullRect);
	}

	int32 ComputeStride(const int32 Count)
	{
		return FMath::Max(1, FMath::DivideAndRoundUp(Count, MaxPolylinePoints));
	}

	void BuildPolyline(const TArray<FVector2f>& Samples,
		const TArray<int32>& ShotSampleIndices,
		const int32 Stride,
		const int32 ShotStride,
		const int32 LastIndex,
		const FVector2f& Centre,
		const float PxPerDeg,
		TArray<FVector2f>& OutPoints)
	{
		OutPoints.Reset();

		const int32 Count = FMath::Min(LastIndex + 1, Samples.Num());
		if (Count < 2)
		{
			return;
		}

		const int32 EffectiveStride = FMath::Max(Stride, 1);
		const int32 EffectiveShotStride = FMath::Max(ShotStride, 1);
		OutPoints.Reserve(Count / EffectiveStride
			+ FMath::DivideAndRoundUp(ShotSampleIndices.Num(), EffectiveShotStride) + 2);

		int32 NextStrideIndex = 0;
		int32 ShotCursor = 0;
		int32 Index = 0;
		int32 EmittedIndex = INDEX_NONE;

		while (Index < Count)
		{
			OutPoints.Add(DegToScreen(Samples[Index], Centre, PxPerDeg));
			EmittedIndex = Index;

			while (NextStrideIndex <= Index)
			{
				NextStrideIndex += EffectiveStride;
			}
			while (ShotSampleIndices.IsValidIndex(ShotCursor) && ShotSampleIndices[ShotCursor] <= Index)
			{
				ShotCursor += EffectiveShotStride;
			}

			Index = ShotSampleIndices.IsValidIndex(ShotCursor)
				? FMath::Min(NextStrideIndex, ShotSampleIndices[ShotCursor])
				: NextStrideIndex;
		}

		if (EmittedIndex != Count - 1)
		{
			OutPoints.Add(DegToScreen(Samples[Count - 1], Centre, PxPerDeg));
		}
	}

	FVector2f LerpSample(const TArray<FVector2f>& Samples, const int32 Index0, const int32 Index1, const float Alpha)
	{
		if (!Samples.IsValidIndex(Index0))
		{
			return FVector2f::ZeroVector;
		}
		if (!Samples.IsValidIndex(Index1))
		{
			return Samples[Index0];
		}
		return FMath::Lerp(Samples[Index0], Samples[Index1], Alpha);
	}

	float LerpSample(const TArray<float>& Samples, const int32 Index0, const int32 Index1, const float Alpha)
	{
		if (!Samples.IsValidIndex(Index0))
		{
			return 0.0f;
		}
		if (!Samples.IsValidIndex(Index1))
		{
			return Samples[Index0];
		}
		return FMath::Lerp(Samples[Index0], Samples[Index1], Alpha);
	}

	void DrawPoint(FSlateWindowElementList& OutDrawElements,
		const int32 Layer,
		const FGeometry& AllottedGeometry,
		const FSlateBrush* Brush,
		const FVector2f& Point,
		const float Size,
		const FLinearColor& Color)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, Layer,
			AllottedGeometry.ToPaintGeometry(FVector2f(Size, Size), FSlateLayoutTransform(Point - FVector2f(Size * 0.5f))),
			Brush, ESlateDrawEffect::None, Color);
	}

	void DrawText(FSlateWindowElementList& OutDrawElements,
		const int32 Layer,
		const FGeometry& AllottedGeometry,
		const FSlateFontInfo& Font,
		const FString& Text,
		const FVector2f& Position,
		const float Width,
		const FLinearColor& Color)
	{
		FSlateDrawElement::MakeText(OutDrawElements, Layer,
			AllottedGeometry.ToPaintGeometry(FVector2f(Width, 14.0f), FSlateLayoutTransform(Position)),
			Text, Font, ESlateDrawEffect::None, Color);
	}
}

void SArcRecoilPatternCanvas::Construct(const FArguments& InArgs)
{
	Pane = InArgs._Pane;
	Result = InArgs._Result;
	Settings = InArgs._Settings;

	SetCanTick(false);
	UpdateScale();
}

void SArcRecoilPatternCanvas::RefreshFromResult(TSharedPtr<const ArcRecoil::Preview::FArcRecoilPreviewResult> InResult)
{
	Result = InResult;
	UpdateScale();
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SArcRecoilPatternCanvas::SetCurrentSample(const float InCurrentSample)
{
	const float NewSample = FMath::IsFinite(InCurrentSample) ? InCurrentSample : 0.0f;
	if (NewSample == CurrentSample)
	{
		return;
	}

	CurrentSample = NewSample;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SArcRecoilPatternCanvas::UpdateScale()
{
	using namespace ArcRecoilPatternCanvasPrivate;

	float ExtentDeg = 1.0f;
	if (Result.IsValid())
	{
		if (Pane == EArcRecoilPreviewPane::Camera)
		{
			ExtentDeg = Result->CameraMaxAbsDeg;
		}
		else
		{
			ExtentDeg = FMath::Max3(Result->ImpactMaxAbsDeg,
				FMath::Abs(Result->BakedProfile.MaxOffsetDeg.X),
				FMath::Abs(Result->BakedProfile.MaxOffsetDeg.Y));
		}
	}

	if (!FMath::IsFinite(ExtentDeg))
	{
		ExtentDeg = 1.0f;
	}

	HalfExtentDeg = FMath::Max(ExtentDeg * MarginFraction, MinHalfExtentDeg);
}

FVector2D SArcRecoilPatternCanvas::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(420.0f, 420.0f);
}

int32 SArcRecoilPatternCanvas::OnPaint(const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	using namespace ArcRecoilPatternCanvasPrivate;

	const int32 BackgroundLayer = LayerId;
	const int32 GridLayer = LayerId + 1;
	const int32 PathLayer = LayerId + 2;
	const int32 PointLayer = LayerId + 3;
	const int32 MarkerLayer = LayerId + 4;
	const int32 TextLayer = LayerId + 5;

	const FSlateBrush* Brush = FCoreStyle::Get().GetBrush("GenericWhiteBox");
	const FSlateFontInfo Font = FCoreStyle::Get().GetFontStyle("SmallFont");
	const FPaintGeometry BaseGeometry = AllottedGeometry.ToPaintGeometry();

	FSlateDrawElement::MakeBox(OutDrawElements, BackgroundLayer, BaseGeometry, Brush,
		ESlateDrawEffect::None, BackgroundColor);

	const FVector2f LocalSize = FVector2f(AllottedGeometry.GetLocalSize());
	const float RadiusPx = FMath::Min(LocalSize.X, LocalSize.Y) * 0.5f - PlotInsetPx;
	if (LocalSize.X < 32.0f || LocalSize.Y < 32.0f || RadiusPx <= 1.0f)
	{
		return TextLayer + 1;
	}

	const FVector2f Centre = LocalSize * 0.5f;
	const float PxPerDeg = RadiusPx / HalfExtentDeg;
	const FSlateRect LocalCullRect = TransformRect(Inverse(AllottedGeometry.GetAccumulatedLayoutTransform()), MyCullingRect);

	const float GridStep = NiceGridStep(HalfExtentDeg);
	const int32 GridLineCount = GridStep > 0.0f
		? FMath::Clamp(FMath::FloorToInt(HalfExtentDeg / GridStep), 0, MaxGridLines)
		: 0;

	TArray<FVector2f> Line;
	Line.Reserve(MaxPolylinePoints + 2);

	for (int32 Index = 1; Index <= GridLineCount; ++Index)
	{
		const float Value = static_cast<float>(Index) * GridStep;
		const float OffsetPx = Value * PxPerDeg;

		for (int32 Side = 0; Side < 2; ++Side)
		{
			const float SignedOffsetPx = Side == 0 ? OffsetPx : -OffsetPx;

			Line.Reset();
			Line.Add(FVector2f(Centre.X + SignedOffsetPx, 0.0f));
			Line.Add(FVector2f(Centre.X + SignedOffsetPx, LocalSize.Y));
			FSlateDrawElement::MakeLines(OutDrawElements, GridLayer, BaseGeometry, Line,
				ESlateDrawEffect::None, GridColor, false, 1.0f);

			Line.Reset();
			Line.Add(FVector2f(0.0f, Centre.Y + SignedOffsetPx));
			Line.Add(FVector2f(LocalSize.X, Centre.Y + SignedOffsetPx));
			FSlateDrawElement::MakeLines(OutDrawElements, GridLayer, BaseGeometry, Line,
				ESlateDrawEffect::None, GridColor, false, 1.0f);
		}

		const FString PositiveLabel = FormatDegrees(Value, GridStep);
		const FString NegativeLabel = FormatDegrees(-Value, GridStep);
		DrawText(OutDrawElements, TextLayer, AllottedGeometry, Font, PositiveLabel,
			FVector2f(Centre.X + OffsetPx + 2.0f, Centre.Y + 2.0f), 48.0f, LabelColor);
		DrawText(OutDrawElements, TextLayer, AllottedGeometry, Font, NegativeLabel,
			FVector2f(Centre.X - OffsetPx + 2.0f, Centre.Y + 2.0f), 48.0f, LabelColor);
		DrawText(OutDrawElements, TextLayer, AllottedGeometry, Font, PositiveLabel,
			FVector2f(Centre.X + 3.0f, Centre.Y - OffsetPx - 14.0f), 48.0f, LabelColor);
		DrawText(OutDrawElements, TextLayer, AllottedGeometry, Font, NegativeLabel,
			FVector2f(Centre.X + 3.0f, Centre.Y + OffsetPx), 48.0f, LabelColor);
	}

	Line.Reset();
	Line.Add(FVector2f(0.0f, Centre.Y));
	Line.Add(FVector2f(LocalSize.X, Centre.Y));
	FSlateDrawElement::MakeLines(OutDrawElements, GridLayer, BaseGeometry, Line,
		ESlateDrawEffect::None, AxisColor, false, 1.0f);

	Line.Reset();
	Line.Add(FVector2f(Centre.X, 0.0f));
	Line.Add(FVector2f(Centre.X, LocalSize.Y));
	FSlateDrawElement::MakeLines(OutDrawElements, GridLayer, BaseGeometry, Line,
		ESlateDrawEffect::None, AxisColor, false, 1.0f);

	const bool bIsCameraPane = Pane == EArcRecoilPreviewPane::Camera;
	DrawText(OutDrawElements, TextLayer, AllottedGeometry, Font,
		bIsCameraPane ? TEXT("camera debt - yaw right / pitch up (deg)") : TEXT("impact - yaw right / pitch up (deg)"),
		FVector2f(6.0f, 4.0f), LocalSize.X - 12.0f, LabelColor);
	DrawText(OutDrawElements, TextLayer, AllottedGeometry, Font,
		FString::Printf(TEXT("range +/-%.2f deg"), HalfExtentDeg),
		FVector2f(6.0f, 18.0f), LocalSize.X - 12.0f, LabelColor);

	if (bIsCameraPane)
	{
		DrawText(OutDrawElements, TextLayer, AllottedGeometry, Font,
			TEXT("assumes zero player compensation"),
			FVector2f(6.0f, LocalSize.Y - 30.0f), LocalSize.X - 12.0f, CaptionColor);
		DrawText(OutDrawElements, TextLayer, AllottedGeometry, Font,
			TEXT("no view pitch clamp - long bursts flatten in game"),
			FVector2f(6.0f, LocalSize.Y - 16.0f), LocalSize.X - 12.0f, CaptionColor);
	}

	if (!Result.IsValid())
	{
		return TextLayer + 1;
	}

	const ArcRecoil::Preview::FArcRecoilPreviewResult& Data = *Result;
	const int32 SampleCount = FMath::Min3(Data.SampleCount, Data.MeanCameraDeg.Num(), Data.MeanAimOffsetDeg.Num());
	if (SampleCount <= 0 || Data.Runs.Num() <= 0)
	{
		return TextLayer + 1;
	}

	const int32 PolylineStride = ComputeStride(SampleCount);
	const int32 ShotStride = ComputeStride(Data.ShotSampleIndices.Num());

	const float ClampedSample = FMath::Clamp(FMath::IsFinite(CurrentSample) ? CurrentSample : 0.0f,
		0.0f, static_cast<float>(SampleCount - 1));
	const int32 Sample0 = FMath::Clamp(FMath::FloorToInt(ClampedSample), 0, SampleCount - 1);
	const int32 Sample1 = FMath::Min(Sample0 + 1, SampleCount - 1);
	const float SampleAlpha = ClampedSample - static_cast<float>(Sample0);

	const UArcRecoilPreviewSettings* SettingsPtr = Settings.Get();
	const bool bShowCameraPaths = SettingsPtr == nullptr || SettingsPtr->bShowCameraPaths;
	const bool bShowAimOffsetCloud = SettingsPtr == nullptr || SettingsPtr->bShowAimOffsetCloud;
	const bool bShowSpread = SettingsPtr == nullptr || SettingsPtr->bShowSpread;
	const bool bShowClampEllipse = SettingsPtr == nullptr || SettingsPtr->bShowClampEllipse;

	if (bIsCameraPane)
	{
		if (bShowCameraPaths)
		{
			for (const ArcRecoil::Preview::FArcRecoilPreviewRun& Run : Data.Runs)
			{
				BuildPolyline(Run.CameraDeg, Data.ShotSampleIndices, PolylineStride, ShotStride, Sample0,
					Centre, PxPerDeg, Line);
				if (IsPolylineVisible(Line, LocalCullRect))
				{
					FSlateDrawElement::MakeLines(OutDrawElements, PathLayer, BaseGeometry, Line,
						ESlateDrawEffect::None, SeedPathColor, true, 1.0f);
				}
			}

			BuildPolyline(Data.MeanCameraDeg, Data.ShotSampleIndices, PolylineStride, ShotStride, Sample0,
				Centre, PxPerDeg, Line);
			if (IsPolylineVisible(Line, LocalCullRect))
			{
				FSlateDrawElement::MakeLines(OutDrawElements, PointLayer, BaseGeometry, Line,
					ESlateDrawEffect::None, MeanPathColor, true, 2.0f);
			}

			if (PolylineStride > 1 || ShotStride > 1)
			{
				const FString DecimationText = ShotStride > 1
					? FString::Printf(TEXT("trace decimated - 1 of every %d samples, 1 of every %d shot starts"),
						PolylineStride, ShotStride)
					: FString::Printf(TEXT("trace decimated - 1 of every %d samples, shot starts kept"),
						PolylineStride);
				DrawText(OutDrawElements, TextLayer, AllottedGeometry, Font, DecimationText,
					FVector2f(6.0f, 32.0f), LocalSize.X - 12.0f, CaptionColor);
			}
		}

		const FVector2f MeanNow = LerpSample(Data.MeanCameraDeg, Sample0, Sample1, SampleAlpha);
		DrawPoint(OutDrawElements, MarkerLayer, AllottedGeometry, Brush,
			DegToScreen(MeanNow, Centre, PxPerDeg), 7.0f, MarkerColor);

		return TextLayer + 1;
	}

	if (bShowClampEllipse)
	{
		const FVector2f MaxOffsetDeg = Data.BakedProfile.MaxOffsetDeg;
		if (MaxOffsetDeg.X > 0.0f && MaxOffsetDeg.Y > 0.0f)
		{
			Line.Reset();
			Line.Reserve(EllipseSegments + 1);
			for (int32 Index = 0; Index <= EllipseSegments; ++Index)
			{
				const float Angle = UE_TWO_PI * static_cast<float>(Index) / static_cast<float>(EllipseSegments);
				Line.Add(DegToScreen(
					FVector2f(MaxOffsetDeg.X * FMath::Sin(Angle), MaxOffsetDeg.Y * FMath::Cos(Angle)),
					Centre, PxPerDeg));
			}
			FSlateDrawElement::MakeLines(OutDrawElements, GridLayer, BaseGeometry, Line,
				ESlateDrawEffect::None, ClampEllipseColor, true, 1.0f);
		}
	}

	if (bShowAimOffsetCloud)
	{
		for (int32 Back = TrailSampleCount; Back >= 1; --Back)
		{
			const int32 TrailSample = Sample0 - Back;
			if (TrailSample < 0)
			{
				continue;
			}

			const float Fade = 1.0f - static_cast<float>(Back) / static_cast<float>(TrailSampleCount + 1);
			const FLinearColor TrailColor(CloudColor.R, CloudColor.G, CloudColor.B, 0.30f * Fade);

			for (const ArcRecoil::Preview::FArcRecoilPreviewRun& Run : Data.Runs)
			{
				if (!Run.AimOffsetDeg.IsValidIndex(TrailSample))
				{
					continue;
				}

				const FVector2f Point = DegToScreen(Run.AimOffsetDeg[TrailSample], Centre, PxPerDeg);
				if (IsPointVisible(Point, LocalCullRect, 2.0f))
				{
					DrawPoint(OutDrawElements, PointLayer, AllottedGeometry, Brush, Point, 2.0f, TrailColor);
				}
			}
		}

		for (const ArcRecoil::Preview::FArcRecoilPreviewRun& Run : Data.Runs)
		{
			const FVector2f Point = DegToScreen(
				LerpSample(Run.AimOffsetDeg, Sample0, Sample1, SampleAlpha), Centre, PxPerDeg);
			if (IsPointVisible(Point, LocalCullRect, 3.0f))
			{
				DrawPoint(OutDrawElements, MarkerLayer, AllottedGeometry, Brush, Point, 3.0f, CloudColor);
			}
		}
	}

	if (bShowSpread)
	{
		// The cone is built around the already-offset direction, so it is centred on the layer-2 mean, not the reticle.
		const FVector2f ConeCentreDeg = LerpSample(Data.MeanAimOffsetDeg, Sample0, Sample1, SampleAlpha);
		const float ConeHalfAngleDeg = FMath::Max(
			LerpSample(Data.SpreadHalfAngleDeg, Sample0, Sample1, SampleAlpha), 0.0f);

		if (ConeHalfAngleDeg > 0.0f)
		{
			Line.Reset();
			Line.Reserve(EllipseSegments + 1);
			for (int32 Index = 0; Index <= EllipseSegments; ++Index)
			{
				const float Angle = UE_TWO_PI * static_cast<float>(Index) / static_cast<float>(EllipseSegments);
				Line.Add(DegToScreen(
					ConeCentreDeg + FVector2f(ConeHalfAngleDeg * FMath::Sin(Angle), ConeHalfAngleDeg * FMath::Cos(Angle)),
					Centre, PxPerDeg));
			}
			FSlateDrawElement::MakeLines(OutDrawElements, PathLayer, BaseGeometry, Line,
				ESlateDrawEffect::None, SpreadCircleColor, true, 1.0f);
		}

		const int32 RequestedSpreadSamples = SettingsPtr != nullptr ? SettingsPtr->SpreadSamplesPerShot : 64;
		const int32 ClampedSpreadSamples = FMath::Clamp(RequestedSpreadSamples, 0, MaxSpreadSamples);
		if (ClampedSpreadSamples == RequestedSpreadSamples)
		{
			LastWarnedSpreadSamples = INDEX_NONE;
		}
		else if (LastWarnedSpreadSamples != RequestedSpreadSamples)
		{
			LastWarnedSpreadSamples = RequestedSpreadSamples;
			UE_LOGF(LogArcRecoil, Warning,"Recoil preview: requested %ls %d is outside [%d, %d]; drawing %d instead.",
				TEXT("spread samples per shot"), RequestedSpreadSamples, 0, MaxSpreadSamples, ClampedSpreadSamples);
		}

		const int32 SpreadSampleCount = Data.MaxSpreadSamples > 0
			? FMath::Min(ClampedSpreadSamples, Data.MaxSpreadSamples)
			: ClampedSpreadSamples;

		const uint32 VizSeed = static_cast<uint32>(SettingsPtr != nullptr ? SettingsPtr->BaseSeed : 0)
			+ VizSeedOffset + static_cast<uint32>(Sample0);
		FRandomStream VizStream(static_cast<int32>(VizSeed));

		for (int32 Index = 0; Index < SpreadSampleCount; ++Index)
		{
			const FVector Dir = ArcRecoil::PerturbAimDirection(FVector::ForwardVector, ConeCentreDeg,
				ConeHalfAngleDeg, Data.BakedProfile.SpreadExponent, VizStream);
			const float YawDeg = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)));
			const float PitchDeg = static_cast<float>(
				FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Dir.Z, -1.0, 1.0))));

			const FVector2f Point = DegToScreen(FVector2f(PitchDeg, YawDeg), Centre, PxPerDeg);
			if (IsPointVisible(Point, LocalCullRect, 2.0f))
			{
				DrawPoint(OutDrawElements, PathLayer, AllottedGeometry, Brush, Point, 2.0f, ImpactPointColor);
			}
		}
	}

	Line.Reset();
	Line.Add(FVector2f(Centre.X - 7.0f, Centre.Y));
	Line.Add(FVector2f(Centre.X + 7.0f, Centre.Y));
	FSlateDrawElement::MakeLines(OutDrawElements, MarkerLayer, BaseGeometry, Line,
		ESlateDrawEffect::None, ReticleColor, false, 1.0f);

	Line.Reset();
	Line.Add(FVector2f(Centre.X, Centre.Y - 7.0f));
	Line.Add(FVector2f(Centre.X, Centre.Y + 7.0f));
	FSlateDrawElement::MakeLines(OutDrawElements, MarkerLayer, BaseGeometry, Line,
		ESlateDrawEffect::None, ReticleColor, false, 1.0f);

	return TextLayer + 1;
}
