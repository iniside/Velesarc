// Copyright Lukasz Baran. All Rights Reserved.

#include "River/ArcWaterRiverSplineModifier.h"

#include "Hash/Blake3.h"
#include "River/ArcWaterRiverModifier.h"

namespace ArcWaterRiverSplineModifierLocal_0af7
{
	static const FGuid FormatVersion(0x26A92B7D, 0xB92546D3, 0x87B583E2, 0x4C44B74B);
	constexpr double GeometryToleranceCm = 1.e-4;

	template <typename T>
	void HashPod(FBlake3& Hash, const T& Value)
	{
		Hash.Update(&Value, sizeof(T));
	}

	bool Fail(const FString& Message, FText& OutError)
	{
		OutError = FText::FromString(Message);
		return false;
	}

	float SmoothStep(float Value)
	{
		const float T = FMath::Clamp(Value, 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	double Cross(const FVector2D& A, const FVector2D& B, const FVector2D& C)
	{
		return (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
	}

	bool TriangleWeights(const FVector2D& Point, const FVector2D& A, const FVector2D& B,
		const FVector2D& C, FVector3f& OutWeights)
	{
		const double Denominator = Cross(A, B, C);
		if (FMath::Abs(Denominator) <= UE_DOUBLE_SMALL_NUMBER)
		{
			return false;
		}
		const double WeightA = Cross(B, C, Point) / Denominator;
		const double WeightB = Cross(C, A, Point) / Denominator;
		const double WeightC = 1.0 - WeightA - WeightB;
		if (WeightA < -GeometryToleranceCm || WeightB < -GeometryToleranceCm ||
			WeightC < -GeometryToleranceCm)
		{
			return false;
		}
		OutWeights = FVector3f(
			static_cast<float>(WeightA), static_cast<float>(WeightB), static_cast<float>(WeightC));
		return true;
	}

	bool IsZeroTaperEndpointSample(const FArcWaterRiverGeneratedReach& Reach,
		const FVector2D& Position)
	{
		if (Reach.bClosed || Reach.Sections.IsEmpty())
		{
			return false;
		}
		const FArcWaterRiverChannelSection& Start = Reach.Sections[0];
		if (Reach.bStartIsOriginalEndpoint && Start.HalfWidthCm <= 0.0f &&
			FVector2D::DistSquared(Position, FVector2D(Start.Position.X, Start.Position.Y)) <=
			FMath::Square(GeometryToleranceCm))
		{
			return true;
		}
		const FArcWaterRiverChannelSection& End = Reach.Sections.Last();
		return Reach.bEndIsOriginalEndpoint && End.HalfWidthCm <= 0.0f &&
			FVector2D::DistSquared(Position, FVector2D(End.Position.X, End.Position.Y)) <=
			FMath::Square(GeometryToleranceCm);
	}

	void GetPixelRange(const FVector2D& MinPosition, const FVector2D& MaxPosition,
		const FVector2D& Origin, const FVector2D& PixelStep, FIntPoint Dims,
		int32& OutMinX, int32& OutMinY, int32& OutMaxX, int32& OutMaxY)
	{
		OutMinX = FMath::Clamp(FMath::FloorToInt((MinPosition.X - Origin.X) / PixelStep.X), 0, Dims.X - 1);
		OutMinY = FMath::Clamp(FMath::FloorToInt((MinPosition.Y - Origin.Y) / PixelStep.Y), 0, Dims.Y - 1);
		OutMaxX = FMath::Clamp(FMath::CeilToInt((MaxPosition.X - Origin.X) / PixelStep.X), 0, Dims.X - 1);
		OutMaxY = FMath::Clamp(FMath::CeilToInt((MaxPosition.Y - Origin.Y) / PixelStep.Y), 0, Dims.Y - 1);
	}

	bool RasterizeCoreTriangle(const FArcWaterRiverGeneratedReach& Reach,
		const FVector2D& A, const FVector2D& B, const FVector2D& C,
		float WaterZA, float WaterZB, float WaterZC,
		float DepthA, float DepthB, float DepthC,
		float AcrossA, float AcrossB, float AcrossC,
		const FVector2D& Origin, const FVector2D& PixelStep, FIntPoint Dims,
		TArray<float>& TargetHeights, TArray<float>& Masks, TArray<uint8>& CoreSamples)
	{
		if (FMath::Abs(Cross(A, B, C)) <= UE_DOUBLE_SMALL_NUMBER)
		{
			return true;
		}
		const FVector2D MinPosition(
			FMath::Min3(A.X, B.X, C.X), FMath::Min3(A.Y, B.Y, C.Y));
		const FVector2D MaxPosition(
			FMath::Max3(A.X, B.X, C.X), FMath::Max3(A.Y, B.Y, C.Y));
		int32 MinX = 0;
		int32 MinY = 0;
		int32 MaxX = 0;
		int32 MaxY = 0;
		GetPixelRange(MinPosition, MaxPosition, Origin, PixelStep, Dims,
			MinX, MinY, MaxX, MaxY);
		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				const FVector2D Position = Origin + FVector2D(PixelStep.X * X, PixelStep.Y * Y);
				FVector3f Weights;
				if (!TriangleWeights(Position, A, B, C, Weights) ||
					IsZeroTaperEndpointSample(Reach, Position))
				{
					continue;
				}
				const float WaterZ = WaterZA * Weights.X + WaterZB * Weights.Y + WaterZC * Weights.Z;
				const float BaseDepth = DepthA * Weights.X + DepthB * Weights.Y + DepthC * Weights.Z;
				const float Across = AcrossA * Weights.X + AcrossB * Weights.Y + AcrossC * Weights.Z;
				const float Depth = BaseDepth * (1.0f - SmoothStep(FMath::Abs(Across)));
				const float BedTarget = WaterZ - FMath::Max(Depth, 0.0f);
				const int32 SampleIndex = Y * Dims.X + X;
				if (CoreSamples[SampleIndex] == 0 || BedTarget < TargetHeights[SampleIndex])
				{
					TargetHeights[SampleIndex] = BedTarget;
				}
				Masks[SampleIndex] = 1.0f;
				CoreSamples[SampleIndex] = 1;
			}
		}
		return true;
	}

	bool RasterizeBankEdge(const FArcWaterRiverGeneratedReach& Reach,
		const FVector2D& A, const FVector2D& B,
		const FArcWaterRiverChannelSection& ProfileA,
		const FArcWaterRiverChannelSection& ProfileB,
		const FVector2D& Origin, const FVector2D& PixelStep, FIntPoint Dims,
		TArray<float>& TargetHeights, TArray<float>& Masks, const TArray<uint8>& CoreSamples)
	{
		const FVector2D EdgeDelta = B - A;
		const double EdgeLengthSquared = EdgeDelta.SizeSquared();
		const float MaximumFalloff = FMath::Max(ProfileA.FalloffCm, ProfileB.FalloffCm);
		if (EdgeLengthSquared <= UE_DOUBLE_SMALL_NUMBER || !(MaximumFalloff > 0.0f))
		{
			return true;
		}
		const FVector2D Expand(MaximumFalloff, MaximumFalloff);
		const FVector2D MinPosition(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y));
		const FVector2D MaxPosition(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y));
		int32 MinX = 0;
		int32 MinY = 0;
		int32 MaxX = 0;
		int32 MaxY = 0;
		GetPixelRange(MinPosition - Expand, MaxPosition + Expand, Origin, PixelStep, Dims,
			MinX, MinY, MaxX, MaxY);
		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				const int32 SampleIndex = Y * Dims.X + X;
				if (CoreSamples[SampleIndex] != 0)
				{
					continue;
				}
				const FVector2D Position = Origin + FVector2D(PixelStep.X * X, PixelStep.Y * Y);
				const float EdgeAlpha = static_cast<float>(FMath::Clamp(
					FVector2D::DotProduct(Position - A, EdgeDelta) / EdgeLengthSquared, 0.0, 1.0));
				const FVector2D ShorePosition = FMath::Lerp(A, B, EdgeAlpha);
				if (IsZeroTaperEndpointSample(Reach, ShorePosition))
				{
					continue;
				}
				const float Falloff = FMath::Lerp(ProfileA.FalloffCm, ProfileB.FalloffCm, EdgeAlpha);
				const float DistanceToShore = static_cast<float>(FVector2D::Distance(Position, ShorePosition));
				if (!(Falloff > 0.0f) || DistanceToShore >= Falloff)
				{
					continue;
				}
				const float WaterZ = FMath::Lerp(ProfileA.Position.Z, ProfileB.Position.Z, EdgeAlpha);
				const float Freeboard = FMath::Lerp(ProfileA.FreeboardCm, ProfileB.FreeboardCm, EdgeAlpha);
				const float HalfFalloff = Falloff * 0.5f;
				float Target = 0.0f;
				float Mask = 0.0f;
				if (DistanceToShore <= HalfFalloff)
				{
					Target = FMath::Lerp(WaterZ, WaterZ + Freeboard,
						SmoothStep(DistanceToShore / HalfFalloff));
					Mask = 1.0f;
				}
				else
				{
					Target = WaterZ + Freeboard;
					Mask = 1.0f - SmoothStep((DistanceToShore - HalfFalloff) / HalfFalloff);
				}
				if (Masks[SampleIndex] <= 0.0f ||
					Target > TargetHeights[SampleIndex] + UE_KINDA_SMALL_NUMBER)
				{
					TargetHeights[SampleIndex] = Target;
					Masks[SampleIndex] = Mask;
				}
			}
		}
		return true;
	}

	bool RasterizeReach(const FArcWaterRiverGeneratedReach& Reach, const FVector2D& CoverageXY,
		FIntPoint Dims, TArray<float>& OutTargetHeights, TArray<float>& OutMasks)
	{
		if (Reach.Sections.Num() < 2 || CoverageXY.X <= 0.0 || CoverageXY.Y <= 0.0 ||
			Dims.X < 2 || Dims.Y < 2)
		{
			return false;
		}
		const int64 SampleCount64 = static_cast<int64>(Dims.X) * static_cast<int64>(Dims.Y);
		if (SampleCount64 <= 0 || SampleCount64 > MAX_int32)
		{
			return false;
		}
		const int32 SampleCount = static_cast<int32>(SampleCount64);
		const FVector2D Origin = CoverageXY * -0.5;
		const FVector2D PixelStep(
			CoverageXY.X / static_cast<double>(Dims.X - 1),
			CoverageXY.Y / static_cast<double>(Dims.Y - 1));
		TArray<float> TargetHeights;
		TArray<float> Masks;
		TArray<uint8> CoreSamples;
		TargetHeights.Init(0.0f, SampleCount);
		Masks.Init(0.0f, SampleCount);
		CoreSamples.Init(0, SampleCount);

		const int32 SegmentCount = Reach.Sections.Num() - 1 + (Reach.bClosed ? 1 : 0);
		for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
		{
			const FArcWaterRiverChannelSection& Start = Reach.Sections[SegmentIndex];
			const FArcWaterRiverChannelSection& End = Reach.Sections[(SegmentIndex + 1) % Reach.Sections.Num()];
			const FVector2D StartCenter(Start.Position.X, Start.Position.Y);
			const FVector2D EndCenter(End.Position.X, End.Position.Y);
			const FVector2D Corners[4] = {
				StartCenter - Start.Lateral * Start.HalfWidthCm,
				StartCenter + Start.Lateral * Start.HalfWidthCm,
				EndCenter + End.Lateral * End.HalfWidthCm,
				EndCenter - End.Lateral * End.HalfWidthCm
			};
			if (!RasterizeCoreTriangle(Reach, Corners[0], Corners[1], Corners[2],
				Start.Position.Z, Start.Position.Z, End.Position.Z,
				Start.DepthCm, Start.DepthCm, End.DepthCm, -1.0f, 1.0f, 1.0f,
				Origin, PixelStep, Dims, TargetHeights, Masks, CoreSamples) ||
				!RasterizeCoreTriangle(Reach, Corners[0], Corners[2], Corners[3],
					Start.Position.Z, End.Position.Z, End.Position.Z,
					Start.DepthCm, End.DepthCm, End.DepthCm, -1.0f, 1.0f, -1.0f,
					Origin, PixelStep, Dims, TargetHeights, Masks, CoreSamples))
			{
				return false;
			}
		}

		for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
		{
			const FArcWaterRiverChannelSection& Start = Reach.Sections[SegmentIndex];
			const FArcWaterRiverChannelSection& End = Reach.Sections[(SegmentIndex + 1) % Reach.Sections.Num()];
			const FVector2D StartCenter(Start.Position.X, Start.Position.Y);
			const FVector2D EndCenter(End.Position.X, End.Position.Y);
			const FVector2D Corners[4] = {
				StartCenter - Start.Lateral * Start.HalfWidthCm,
				StartCenter + Start.Lateral * Start.HalfWidthCm,
				EndCenter + End.Lateral * End.HalfWidthCm,
				EndCenter - End.Lateral * End.HalfWidthCm
			};
			if (!RasterizeBankEdge(Reach, Corners[0], Corners[3], Start, End,
				Origin, PixelStep, Dims, TargetHeights, Masks, CoreSamples) ||
				!RasterizeBankEdge(Reach, Corners[1], Corners[2], Start, End,
					Origin, PixelStep, Dims, TargetHeights, Masks, CoreSamples))
			{
				return false;
			}
		}

		if (!Reach.bClosed)
		{
			for (const FArcWaterRiverChannelSection* Profile : { &Reach.Sections[0], &Reach.Sections.Last() })
			{
				const FVector2D Center(Profile->Position.X, Profile->Position.Y);
				const FVector2D A = Center - Profile->Lateral * Profile->HalfWidthCm;
				const FVector2D B = Center + Profile->Lateral * Profile->HalfWidthCm;
				if (!RasterizeBankEdge(Reach, A, B, *Profile, *Profile,
					Origin, PixelStep, Dims, TargetHeights, Masks, CoreSamples))
				{
					return false;
				}
			}
		}

		OutTargetHeights = MoveTemp(TargetHeights);
		OutMasks = MoveTemp(Masks);
		return true;
	}

	void HashSection(FBlake3& Hash, const FArcWaterRiverChannelSection& Section)
	{
		HashPod(Hash, Section.Position.X);
		HashPod(Hash, Section.Position.Y);
		HashPod(Hash, Section.Position.Z);
		HashPod(Hash, Section.Lateral.X);
		HashPod(Hash, Section.Lateral.Y);
		HashPod(Hash, Section.HalfWidthCm);
		HashPod(Hash, Section.DepthCm);
		HashPod(Hash, Section.FalloffCm);
		HashPod(Hash, Section.FreeboardCm);
	}

	FGuid MakeContentId(const FArcWaterRiverGeneratedReach& Reach, const FVector2D& CoverageXY,
		FIntPoint Dims, double SampleSpacingCm, const ANSICHAR* Salt)
	{
		FBlake3 Hash;
		HashPod(Hash, FormatVersion);
		HashPod(Hash, CoverageXY.X);
		HashPod(Hash, CoverageXY.Y);
		HashPod(Hash, Dims.X);
		HashPod(Hash, Dims.Y);
		HashPod(Hash, SampleSpacingCm);
		HashPod(Hash, Reach.bClosed);
		HashPod(Hash, Reach.bStartIsOriginalEndpoint);
		HashPod(Hash, Reach.bEndIsOriginalEndpoint);
		const int32 SectionCount = Reach.Sections.Num();
		HashPod(Hash, SectionCount);
		for (const FArcWaterRiverChannelSection& Section : Reach.Sections)
		{
			HashSection(Hash, Section);
		}
		Hash.Update(Salt, FCStringAnsi::Strlen(Salt));
		const FBlake3Hash Digest = Hash.Finalize();
		uint32 Words[4] = { 0u, 0u, 0u, 0u };
		FMemory::Memcpy(Words, Digest.GetBytes(), sizeof(Words));
		return FGuid(Words[0], Words[1], Words[2], Words[3]);
	}
}

struct FArcWaterRiverSplineFieldScratch
{
	FArcWaterRiverGeneratedReach Reach;
	FVector2D CoverageXY = FVector2D::ZeroVector;
	FIntPoint Dims = FIntPoint::ZeroValue;
	FGuid HeightContentId;

	bool ProvideTargetHeights(TArray<float>& OutValues, FIntPoint& OutDims)
	{
		Build();
		if (!bBuilt)
		{
			return false;
		}
		OutValues = TargetHeights;
		OutDims = Dims;
		return true;
	}

	bool ProvideMasks(TArray<float>& OutValues, FIntPoint& OutDims)
	{
		Build();
		if (!bBuilt)
		{
			return false;
		}
		OutValues = Masks;
		OutDims = Dims;
		return true;
	}

private:
	void Build()
	{
		if (bAttempted)
		{
			return;
		}
		bAttempted = true;
		bBuilt = ArcWaterRiverSplineModifierLocal_0af7::RasterizeReach(
			Reach, CoverageXY, Dims, TargetHeights, Masks);
	}

	TArray<float> TargetHeights;
	TArray<float> Masks;
	bool bAttempted = false;
	bool bBuilt = false;
};

UArcWaterRiverSplineModifier::UArcWaterRiverSplineModifier()
{
	BlendMode = EArcTerrainBlendMode::Min;
}

bool UArcWaterRiverSplineModifier::Initialize(UArcWaterRiverModifier& Parent,
	const FArcWaterRiverGeneratedReach& InReach, double InSampleSpacingCm, FText& OutError)
{
	using namespace ArcWaterRiverSplineModifierLocal_0af7;

	OutError = FText::GetEmpty();
	if (InReach.Sections.Num() < 2 || !FMath::IsFinite(InSampleSpacingCm) || InSampleSpacingCm <= 0.0)
	{
		return Fail(TEXT("River spline terrain modifier requires a valid reach and terrain texel spacing."), OutError);
	}

	FBox2D InfluenceBounds(ForceInit);
	for (const FArcWaterRiverChannelSection& Section : InReach.Sections)
	{
		const double Radius = static_cast<double>(Section.HalfWidthCm + Section.FalloffCm);
		if (Section.Position.ContainsNaN() || !FMath::IsFinite(Radius) || Radius < 0.0)
		{
			return Fail(TEXT("River spline terrain modifier received a non-finite channel section."), OutError);
		}
		const FVector2D Center(Section.Position.X, Section.Position.Y);
		InfluenceBounds += Center - FVector2D(Radius, Radius);
		InfluenceBounds += Center + FVector2D(Radius, Radius);
	}
	InfluenceBounds = InfluenceBounds.ExpandBy(InSampleSpacingCm * 2.0);
	if (!InfluenceBounds.bIsValid)
	{
		return Fail(TEXT("River spline terrain modifier produced invalid coverage."), OutError);
	}

	const FVector2D Center = InfluenceBounds.GetCenter();
	const FVector2D DerivedCoverage = InfluenceBounds.GetSize();
	const double DimsX = FMath::CeilToDouble(DerivedCoverage.X / InSampleSpacingCm) + 1.0;
	const double DimsY = FMath::CeilToDouble(DerivedCoverage.Y / InSampleSpacingCm) + 1.0;
	if (!FMath::IsFinite(DimsX) || !FMath::IsFinite(DimsY) || DimsX < 2.0 || DimsY < 2.0 ||
		DimsX > static_cast<double>(MAX_int32) || DimsY > static_cast<double>(MAX_int32))
	{
		return Fail(TEXT("River spline terrain modifier exceeds the supported field dimensions."), OutError);
	}
	const FIntPoint DerivedDims(static_cast<int32>(DimsX), static_cast<int32>(DimsY));
	const int64 SampleCount = static_cast<int64>(DerivedDims.X) * static_cast<int64>(DerivedDims.Y);
	if (SampleCount <= 0 || SampleCount > MAX_int32)
	{
		return Fail(TEXT("River spline terrain modifier exceeds the supported field sample count."), OutError);
	}

	ParentModifier = &Parent;
	AffectedTerrain = Parent.AffectedTerrain;
	Priority = Parent.Priority;
	BlendMode = EArcTerrainBlendMode::Min;
	MinMaxSoftness = 0.0f;
	bIsDisabled = Parent.bIsDisabled;
	CoverageXY = DerivedCoverage;
	FieldDims = DerivedDims;
	SampleSpacingCm = InSampleSpacingCm;
	Reach = InReach;
	for (FVector& Point : Reach.SplinePoints)
	{
		Point.X -= Center.X;
		Point.Y -= Center.Y;
	}
	for (FArcWaterRiverChannelSection& Section : Reach.Sections)
	{
		Section.Position.X -= Center.X;
		Section.Position.Y -= Center.Y;
	}
	SetRelativeLocation(FVector(Center, 0.0));
	HeightContentId = MakeContentId(Reach, CoverageXY, FieldDims, SampleSpacingCm, "Height");
	MaskContentId = MakeContentId(Reach, CoverageXY, FieldDims, SampleSpacingCm, "Mask");
	FieldScratch.Reset();
	return HeightContentId.IsValid() && MaskContentId.IsValid();
}

int64 UArcWaterRiverSplineModifier::GetEstimatedRasterBytes() const
{
	return static_cast<int64>(FieldDims.X) * static_cast<int64>(FieldDims.Y) * 2ll * sizeof(float);
}

TSharedRef<FArcWaterRiverSplineFieldScratch> UArcWaterRiverSplineModifier::AcquireScratch() const
{
	if (FieldScratch.IsValid() && FieldScratch->HeightContentId == HeightContentId)
	{
		return FieldScratch.ToSharedRef();
	}
	TSharedRef<FArcWaterRiverSplineFieldScratch> Scratch = MakeShared<FArcWaterRiverSplineFieldScratch>();
	Scratch->Reach = Reach;
	Scratch->CoverageXY = CoverageXY;
	Scratch->Dims = FieldDims;
	Scratch->HeightContentId = HeightContentId;
	FieldScratch = Scratch;
	return Scratch;
}

bool UArcWaterRiverSplineModifier::GetSourceDescriptor(FArcStampSourceDesc& OutDesc) const
{
	if (Reach.Sections.Num() < 2 || FieldDims.X < 2 || FieldDims.Y < 2 ||
		!HeightContentId.IsValid() || !MaskContentId.IsValid())
	{
		return false;
	}
	const TSharedRef<FArcWaterRiverSplineFieldScratch> Scratch = AcquireScratch();
	OutDesc = FArcStampSourceDesc();
	OutDesc.Kind = EArcStampSourceKind::BakedField;
	OutDesc.ContentId = HeightContentId;
	OutDesc.SourceDims = FieldDims;
	OutDesc.BakedProvider = [Scratch](TArray<float>& OutValues, FIntPoint& OutDims) -> bool
	{
		return Scratch->ProvideTargetHeights(OutValues, OutDims);
	};
	OutDesc.MaskKind = EArcStampSourceKind::BakedField;
	OutDesc.MaskContentId = MaskContentId;
	OutDesc.MaskSourceDims = FieldDims;
	OutDesc.MaskBakedProvider = [Scratch](TArray<float>& OutValues, FIntPoint& OutDims) -> bool
	{
		return Scratch->ProvideMasks(OutValues, OutDims);
	};
	return true;
}

const UArcTerrainStampComponent* UArcWaterRiverSplineModifier::GetCompositeOrderAnchor() const
{
	return ParentModifier;
}

void UArcWaterRiverSplineModifier::OnUnregister()
{
	FieldScratch.Reset();
	Super::OnUnregister();
}

void UArcWaterRiverSplineModifier::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	FieldScratch.Reset();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}
