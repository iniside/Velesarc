// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcTownTensorField.h"

#include "Simulation/ArcTownSiteFields.h"
#include "Town/ArcTownSiteSnapshot.h"

namespace ArcTownTensorFieldPrivate
{
namespace
{
	struct FTensor2
	{
		double A = 0.0;
		double B = 0.0;
	};

	// Summing tensors rather than angles: a direction and its opposite are the same road.
	void AddDirection(FTensor2& Tensor, const double AngleRad, const double Weight)
	{
		if (Weight <= 0.0 || !FMath::IsFinite(Weight) || !FMath::IsFinite(AngleRad))
		{
			return;
		}

		Tensor.A += Weight * FMath::Cos(2.0 * AngleRad);
		Tensor.B += Weight * FMath::Sin(2.0 * AngleRad);
	}

	void AddVector(FTensor2& Tensor, const FVector2D& Direction, const double Weight)
	{
		const double LengthSquared = Direction.X * Direction.X + Direction.Y * Direction.Y;
		if (LengthSquared <= UE_DOUBLE_SMALL_NUMBER)
		{
			return;
		}

		AddDirection(Tensor, FMath::Atan2(Direction.Y, Direction.X), Weight);
	}

	bool HasFields(const FArcTownTensorField& Field, FIntPoint& OutSiteDims)
	{
		if (Field.Site == nullptr || Field.Fields == nullptr)
		{
			return false;
		}

		OutSiteDims = Field.Site->GetSiteDims();
		if (OutSiteDims.X <= 0 || OutSiteDims.Y <= 0)
		{
			return false;
		}

		const int64 CellCount = static_cast<int64>(Field.Site->GridDims.X) * static_cast<int64>(Field.Site->GridDims.Y);
		return static_cast<int64>(Field.Site->Heights.Num()) == CellCount
			&& static_cast<int64>(Field.Fields->Slope.Num()) == CellCount;
	}

	double HeightAt(const FArcTownSiteSnapshot& Site, const FIntPoint& SiteDims, const int32 SiteX, const int32 SiteY)
	{
		const int32 ClampedX = FMath::Clamp(SiteX, -Site.MarginCells, SiteDims.X - 1 + Site.MarginCells);
		const int32 ClampedY = FMath::Clamp(SiteY, -Site.MarginCells, SiteDims.Y - 1 + Site.MarginCells);
		return static_cast<double>(Site.Heights[Site.SnapshotIndex(ClampedX, ClampedY)]);
	}

	FVector2f NormalizeOrZero(const FVector2f Direction)
	{
		if (!FMath::IsFinite(Direction.X) || !FMath::IsFinite(Direction.Y))
		{
			return FVector2f::ZeroVector;
		}

		const double X = static_cast<double>(Direction.X);
		const double Y = static_cast<double>(Direction.Y);
		const double LengthSquared = X * X + Y * Y;
		if (LengthSquared <= UE_DOUBLE_SMALL_NUMBER)
		{
			return FVector2f::ZeroVector;
		}

		const double Length = FMath::Sqrt(LengthSquared);
		return FVector2f(static_cast<float>(X / Length), static_cast<float>(Y / Length));
	}

	double TerrainWeight(const FArcTownTensorFieldSettings& Settings, const double Slope)
	{
		const double HalfWeightSlope = FMath::Tan(FMath::DegreesToRadians(Settings.TerrainHalfWeightSlopeDeg));
		const double Denominator = Slope + HalfWeightSlope;
		if (!FMath::IsFinite(HalfWeightSlope) || !(HalfWeightSlope > 0.0) || !(Denominator > 0.0))
		{
			return 0.0;
		}

		return Settings.BoundaryWeight * (Slope / Denominator);
	}

	bool EvaluateMajorAngle(const FArcTownTensorField& Field, const FVector2f TownLocalXY, double& OutMajorAngleRad)
	{
		OutMajorAngleRad = 0.0;

		FIntPoint SiteDims = FIntPoint::ZeroValue;
		if (!HasFields(Field, SiteDims) || !FMath::IsFinite(TownLocalXY.X) || !FMath::IsFinite(TownLocalXY.Y))
		{
			return false;
		}

		const FVector2D SamplePoint(static_cast<double>(TownLocalXY.X), static_cast<double>(TownLocalXY.Y));
		const FVector2D CorePoint(
			static_cast<double>(Field.CoreTownLocalXY.X), static_cast<double>(Field.CoreTownLocalXY.Y));
		const FVector2D ToSample = SamplePoint - CorePoint;
		const double CoreFalloff = ArcTownTensorField::Falloff(ToSample.Size(), Field.Settings.CoreFalloffCm);

		FTensor2 Tensor;
		AddVector(Tensor, ToSample, Field.Settings.RadialWeight * CoreFalloff);
		AddDirection(Tensor, FMath::DegreesToRadians(Field.Settings.GridAngleDeg), Field.Settings.GridWeight * CoreFalloff);

		for (const FArcTownFrozenAttractor& Attractor : Field.Attractors)
		{
			const FVector2D AttractorPoint(
				static_cast<double>(Attractor.CenterTownLocalXY.X), static_cast<double>(Attractor.CenterTownLocalXY.Y));
			const FVector2D FromAttractor = SamplePoint - AttractorPoint;
			const double Weight =
				static_cast<double>(Attractor.Strength) * ArcTownTensorField::Falloff(FromAttractor.Size(), Attractor.InfluenceRadiusCm);
			AddVector(Tensor, FromAttractor, Weight);
		}

		FIntPoint Cell = FIntPoint::ZeroValue;
		if (ArcTownSiteFrame::TownLocalXYToCell(*Field.Site, TownLocalXY, Cell))
		{
			const double HeightEast = HeightAt(*Field.Site, SiteDims, Cell.X + 1, Cell.Y);
			const double HeightWest = HeightAt(*Field.Site, SiteDims, Cell.X - 1, Cell.Y);
			const double HeightNorth = HeightAt(*Field.Site, SiteDims, Cell.X, Cell.Y + 1);
			const double HeightSouth = HeightAt(*Field.Site, SiteDims, Cell.X, Cell.Y - 1);
			const FVector2D Gradient(
				(HeightEast - HeightWest) / (2.0 * Field.Site->SampleSpacing),
				(HeightNorth - HeightSouth) / (2.0 * Field.Site->SampleSpacing));

			const double Slope =
				FMath::Max(0.0, static_cast<double>(Field.Fields->Slope[Field.Site->SnapshotIndex(Cell.X, Cell.Y)]));
			AddVector(Tensor, FVector2D(-Gradient.Y, Gradient.X), TerrainWeight(Field.Settings, Slope));
		}

		const double Magnitude = FMath::Sqrt(Tensor.A * Tensor.A + Tensor.B * Tensor.B);
		if (!FMath::IsFinite(Magnitude) || Magnitude <= UE_DOUBLE_SMALL_NUMBER)
		{
			return false;
		}

		// The tensor carries doubled angles, so halving Atan2 recovers the axis.
		OutMajorAngleRad = 0.5 * FMath::Atan2(Tensor.B, Tensor.A);
		return true;
	}
}
}

FVector2f ArcTownSiteFrame::CellToTownLocalXY(const FArcTownSiteSnapshot& Site, const int32 SiteX, const int32 SiteY)
{
	const FIntPoint SiteDims = Site.GetSiteDims();
	const double CenterX = 0.5 * static_cast<double>(SiteDims.X - 1);
	const double CenterY = 0.5 * static_cast<double>(SiteDims.Y - 1);
	return FVector2f(
		static_cast<float>((static_cast<double>(SiteX) - CenterX) * Site.SampleSpacing),
		static_cast<float>((static_cast<double>(SiteY) - CenterY) * Site.SampleSpacing));
}

bool ArcTownSiteFrame::TownLocalXYToCell(const FArcTownSiteSnapshot& Site, const FVector2f TownLocalXY, FIntPoint& OutCell)
{
	OutCell = FIntPoint::ZeroValue;

	const FIntPoint SiteDims = Site.GetSiteDims();
	if (SiteDims.X <= 0 || SiteDims.Y <= 0 || !FMath::IsFinite(Site.SampleSpacing) || Site.SampleSpacing <= 0.0)
	{
		return false;
	}

	if (!FMath::IsFinite(TownLocalXY.X) || !FMath::IsFinite(TownLocalXY.Y))
	{
		return false;
	}

	const double CenterX = 0.5 * static_cast<double>(SiteDims.X - 1);
	const double CenterY = 0.5 * static_cast<double>(SiteDims.Y - 1);
	const double CellX = static_cast<double>(TownLocalXY.X) / Site.SampleSpacing + CenterX;
	const double CellY = static_cast<double>(TownLocalXY.Y) / Site.SampleSpacing + CenterY;
	if (CellX < -0.5 || CellY < -0.5
		|| CellX > static_cast<double>(SiteDims.X - 1) + 0.5
		|| CellY > static_cast<double>(SiteDims.Y - 1) + 0.5)
	{
		return false;
	}

	OutCell = FIntPoint(
		FMath::Clamp(FMath::RoundToInt32(CellX), 0, SiteDims.X - 1),
		FMath::Clamp(FMath::RoundToInt32(CellY), 0, SiteDims.Y - 1));
	return true;
}

bool ArcTownSiteFrame::IsInsideSite(const FArcTownSiteSnapshot& Site, const FVector2f TownLocalXY)
{
	FIntPoint Cell = FIntPoint::ZeroValue;
	return TownLocalXYToCell(Site, TownLocalXY, Cell);
}

FVector2D ArcTownSiteFrame::TownLocalOriginInTerrainLocal(const FArcTownSiteSnapshot& Site)
{
	const FIntPoint SiteDims = Site.GetSiteDims();
	const FVector2D SiteOrigin = Site.GetSiteOriginLocal();
	return FVector2D(
		SiteOrigin.X + 0.5 * static_cast<double>(SiteDims.X - 1) * Site.SampleSpacing,
		SiteOrigin.Y + 0.5 * static_cast<double>(SiteDims.Y - 1) * Site.SampleSpacing);
}

FVector2D ArcTownSiteFrame::TownLocalToTerrainLocalXY(const FArcTownSiteSnapshot& Site, const FVector2D& TownLocalXY)
{
	return TownLocalXY + TownLocalOriginInTerrainLocal(Site);
}

FVector2D ArcTownSiteFrame::TerrainLocalToTownLocalXY(const FArcTownSiteSnapshot& Site, const FVector2D& TerrainLocalXY)
{
	return TerrainLocalXY - TownLocalOriginInTerrainLocal(Site);
}

bool ArcTownSiteFrame::HasUsableExtentXY(const FBox2D& Bounds)
{
	return FMath::IsFinite(Bounds.Min.X)
		&& FMath::IsFinite(Bounds.Min.Y)
		&& FMath::IsFinite(Bounds.Max.X)
		&& FMath::IsFinite(Bounds.Max.Y)
		&& Bounds.Max.X > Bounds.Min.X
		&& Bounds.Max.Y > Bounds.Min.Y;
}

bool ArcTownSiteFrame::HasUsableExtentXY(const FBox& Bounds)
{
	return HasUsableExtentXY(
		FBox2D(FVector2D(Bounds.Min.X, Bounds.Min.Y), FVector2D(Bounds.Max.X, Bounds.Max.Y)));
}

double ArcTownTensorField::Falloff(const double DistanceCm, const double RadiusCm)
{
	if (!FMath::IsFinite(DistanceCm) || !FMath::IsFinite(RadiusCm) || RadiusCm <= 0.0 || DistanceCm < 0.0)
	{
		return 0.0;
	}

	const double Normalized = DistanceCm / RadiusCm;
	return FMath::Exp(-Normalized * Normalized);
}

FVector2f ArcTownTensorField::SampleMajorEigenvector(
	const FArcTownTensorField& Field, const FVector2f TownLocalXY, bool& bOutDegenerate)
{
	using namespace ArcTownTensorFieldPrivate;

	double MajorAngleRad = 0.0;
	bOutDegenerate = !EvaluateMajorAngle(Field, TownLocalXY, MajorAngleRad);
	if (bOutDegenerate)
	{
		return FVector2f::ZeroVector;
	}

	return FVector2f(
		static_cast<float>(FMath::Cos(MajorAngleRad)), static_cast<float>(FMath::Sin(MajorAngleRad)));
}

FVector2f ArcTownTensorField::SampleMajorDirection(
	const FArcTownTensorField& Field, const FVector2f TownLocalXY, const FVector2f IncomingDirection)
{
	const FVector2f Heading = ArcTownTensorFieldPrivate::NormalizeOrZero(IncomingDirection);
	checkf(!Heading.IsZero(),
		TEXT("ArcTownTensorField::SampleMajorDirection needs a finite, non-zero IncomingDirection; the caller passed (%f, %f)."),
		IncomingDirection.X, IncomingDirection.Y);

	bool bDegenerate = false;
	const FVector2f Eigenvector = SampleMajorEigenvector(Field, TownLocalXY, bDegenerate);
	return bDegenerate ? Heading : Eigenvector;
}

FVector2f ArcTownTensorField::SampleMinorDirection(
	const FArcTownTensorField& Field, const FVector2f TownLocalXY, const FVector2f IncomingDirection)
{
	const FVector2f MajorDirection = SampleMajorDirection(Field, TownLocalXY, IncomingDirection);
	return FVector2f(-MajorDirection.Y, MajorDirection.X);
}
