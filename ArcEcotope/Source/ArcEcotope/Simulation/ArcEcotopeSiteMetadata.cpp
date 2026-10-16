// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcEcotopeSiteMetadata.h"

#include "Simulation/ArcEcotopeSiteChunkStorage.h"

namespace ArcEcotopeSiteMetadataPrivate
{
	constexpr int32 CurrentSiteSchemaVersion = 15;
}

int32 FArcEcotopeSiteMetadata::GetCurrentSiteSchemaVersion()
{
	return ArcEcotopeSiteMetadataPrivate::CurrentSiteSchemaVersion;
}

uint8 FArcEcotopeSiteMetadata::GetFieldLevel(const EArcEcotopeSiteField Field) const
{
	switch (Field)
	{
	case EArcEcotopeSiteField::Moisture: return MoistureLevel;
	case EArcEcotopeSiteField::Fertility: return FertilityLevel;
	case EArcEcotopeSiteField::SunLoad: return SunLoadLevel;
	case EArcEcotopeSiteField::WindExposure: return WindExposureLevel;
	case EArcEcotopeSiteField::SkyViewFactor: return SkyViewFactorLevel;
	case EArcEcotopeSiteField::Temperature: return TemperatureLevel;
	case EArcEcotopeSiteField::ModificationCut: return ModificationCutLevel;
	case EArcEcotopeSiteField::RecordValid: return RecordGridLevel;
	case EArcEcotopeSiteField::BareRock: return BareRockLevel;
	case EArcEcotopeSiteField::ChannelExclusion: return ChannelExclusionLevel;
	case EArcEcotopeSiteField::EstablishmentWear: return EstablishmentWearLevel;
	case EArcEcotopeSiteField::SubstrateMobility: return SubstrateMobilityLevel;
	default: return 0;
	}
}

int32 FArcEcotopeSiteMetadata::GetCellCount() const
{
	if (GridDimensions.X <= 0 || GridDimensions.Y <= 0)
	{
		return 0;
	}

	const int64 Count = static_cast<int64>(GridDimensions.X) * GridDimensions.Y;
	return Count <= MAX_int32 ? static_cast<int32>(Count) : 0;
}

bool FArcEcotopeSiteMetadata::TryGetCellIndex(
	const int32 CellX,
	const int32 CellY,
	int32& OutCellIndex) const
{
	if (CellX < 0 || CellY < 0 || GridDimensions.X <= 0 || GridDimensions.Y <= 0
		|| CellX >= GridDimensions.X || CellY >= GridDimensions.Y)
	{
		return false;
	}

	const int64 CellCount = static_cast<int64>(GridDimensions.X) * GridDimensions.Y;
	const int64 CellIndex = static_cast<int64>(CellX) + static_cast<int64>(CellY) * GridDimensions.X;
	if (CellCount <= 0 || CellCount > MAX_int32 || CellIndex < 0 || CellIndex >= CellCount || CellIndex > MAX_int32)
	{
		return false;
	}

	OutCellIndex = static_cast<int32>(CellIndex);
	return true;
}

bool FArcEcotopeSiteMetadata::TryGetCellCoordinates(
	const int32 CellIndex,
	FIntPoint& OutCellCoordinates) const
{
	if (GridDimensions.X <= 0 || GridDimensions.Y <= 0)
	{
		return false;
	}

	const int64 CellCount = static_cast<int64>(GridDimensions.X) * GridDimensions.Y;
	if (CellCount <= 0 || CellCount > MAX_int32 || CellIndex < 0 || static_cast<int64>(CellIndex) >= CellCount)
	{
		return false;
	}

	OutCellCoordinates.X = CellIndex % GridDimensions.X;
	OutCellCoordinates.Y = CellIndex / GridDimensions.X;
	return true;
}

bool FArcEcotopeSiteMetadata::TryGetCellIndexForLocalPosition(
	const FVector2D& LocalPosition,
	int32& OutCellIndex) const
{
	if (!FMath::IsFinite(LocalPosition.X) || !FMath::IsFinite(LocalPosition.Y)
		|| !FMath::IsFinite(CellSizeCm) || CellSizeCm <= 0.0f
		|| !LocalBounds.IsValid
		|| LocalPosition.X < LocalBounds.Min.X || LocalPosition.X > LocalBounds.Max.X
		|| LocalPosition.Y < LocalBounds.Min.Y || LocalPosition.Y > LocalBounds.Max.Y)
	{
		return false;
	}

	// Floor from the minimum edge, then clamp so points on the inclusive maximum
	// edge stay in the final cell.
	const int32 CellX = FMath::Clamp(
		FMath::FloorToInt((LocalPosition.X - LocalBounds.Min.X) / CellSizeCm), 0, GridDimensions.X - 1);
	const int32 CellY = FMath::Clamp(
		FMath::FloorToInt((LocalPosition.Y - LocalBounds.Min.Y) / CellSizeCm), 0, GridDimensions.Y - 1);
	return TryGetCellIndex(CellX, CellY, OutCellIndex);
}

FVector FArcEcotopeSiteMetadata::GetCellCenterLocal(const int32 CellX, const int32 CellY) const
{
	int32 UnusedCellIndex = INDEX_NONE;
	if (!TryGetCellIndex(CellX, CellY, UnusedCellIndex))
	{
		return FVector::ZeroVector;
	}

	const double CellMinX = LocalBounds.Min.X + static_cast<double>(CellX) * CellSizeCm;
	const double CellMinY = LocalBounds.Min.Y + static_cast<double>(CellY) * CellSizeCm;
	const double CellMaxX = FMath::Min(CellMinX + CellSizeCm, static_cast<double>(LocalBounds.Max.X));
	const double CellMaxY = FMath::Min(CellMinY + CellSizeCm, static_cast<double>(LocalBounds.Max.Y));
	const double X = (CellMinX + CellMaxX) * 0.5;
	const double Y = (CellMinY + CellMaxY) * 0.5;
	return FVector(X, Y, 0.0);
}

FIntPoint FArcEcotopeSiteMetadata::GetLevelDimensions(const uint8 Level) const
{
	if (Level > MaxGridLevel || GridDimensions.X <= 0 || GridDimensions.Y <= 0)
	{
		return FIntPoint::ZeroValue;
	}

	const int32 Divisor = static_cast<int32>(1) << Level;
	return FIntPoint(
		FMath::DivideAndRoundUp(GridDimensions.X, Divisor),
		FMath::DivideAndRoundUp(GridDimensions.Y, Divisor));
}

int32 FArcEcotopeSiteMetadata::GetLevelCellCount(const uint8 Level) const
{
	const FIntPoint LevelDimensions = GetLevelDimensions(Level);
	if (LevelDimensions.X <= 0 || LevelDimensions.Y <= 0)
	{
		return 0;
	}

	const int64 Count = static_cast<int64>(LevelDimensions.X) * LevelDimensions.Y;
	return Count <= MAX_int32 ? static_cast<int32>(Count) : 0;
}

double FArcEcotopeSiteMetadata::GetLevelCellSizeCm(const uint8 Level) const
{
	if (Level > MaxGridLevel || !FMath::IsFinite(CellSizeCm) || CellSizeCm <= 0.0f)
	{
		return 0.0;
	}

	return static_cast<double>(CellSizeCm) * static_cast<double>(static_cast<int32>(1) << Level);
}

bool FArcEcotopeSiteMetadata::TryGetLevelCellIndex(
	const FVector2D& LocalPosition,
	const uint8 Level,
	int32& OutIndex) const
{
	const FIntPoint LevelDimensions = GetLevelDimensions(Level);
	const double LevelCellSizeCm = GetLevelCellSizeCm(Level);
	if (LevelDimensions.X <= 0 || LevelDimensions.Y <= 0 || LevelCellSizeCm <= 0.0
		|| !FMath::IsFinite(LocalPosition.X) || !FMath::IsFinite(LocalPosition.Y)
		|| !LocalBounds.IsValid
		|| LocalPosition.X < LocalBounds.Min.X || LocalPosition.X > LocalBounds.Max.X
		|| LocalPosition.Y < LocalBounds.Min.Y || LocalPosition.Y > LocalBounds.Max.Y)
	{
		return false;
	}

	const int32 LevelCellX = FMath::Clamp(
		FMath::FloorToInt32((LocalPosition.X - LocalBounds.Min.X) / LevelCellSizeCm), 0, LevelDimensions.X - 1);
	const int32 LevelCellY = FMath::Clamp(
		FMath::FloorToInt32((LocalPosition.Y - LocalBounds.Min.Y) / LevelCellSizeCm), 0, LevelDimensions.Y - 1);

	const int64 LevelCellCount = static_cast<int64>(LevelDimensions.X) * LevelDimensions.Y;
	const int64 LevelCellIndex = static_cast<int64>(LevelCellX) + static_cast<int64>(LevelCellY) * LevelDimensions.X;
	if (LevelCellCount > MAX_int32 || LevelCellIndex < 0 || LevelCellIndex >= LevelCellCount)
	{
		return false;
	}

	OutIndex = static_cast<int32>(LevelCellIndex);
	return true;
}

FVector FArcEcotopeSiteMetadata::GetLevelCellCenterLocal(
	const int32 LevelCellIndex,
	const uint8 Level) const
{
	FVector2D CellMinLocal = FVector2D::ZeroVector;
	FVector2D CellMaxLocal = FVector2D::ZeroVector;
	if (!TryGetLevelCellRect(LevelCellIndex, Level, CellMinLocal, CellMaxLocal))
	{
		return FVector::ZeroVector;
	}

	return FVector((CellMinLocal.X + CellMaxLocal.X) * 0.5, (CellMinLocal.Y + CellMaxLocal.Y) * 0.5, 0.0);
}

bool FArcEcotopeSiteMetadata::TryGetLevelCellRect(
	const int32 LevelCellIndex,
	const uint8 Level,
	FVector2D& OutMinLocal,
	FVector2D& OutMaxLocal) const
{
	const FIntPoint LevelDimensions = GetLevelDimensions(Level);
	const int32 LevelCellCount = GetLevelCellCount(Level);
	const double LevelCellSizeCm = GetLevelCellSizeCm(Level);
	if (LevelCellCount <= 0 || LevelCellSizeCm <= 0.0 || !LocalBounds.IsValid
		|| LevelCellIndex < 0 || LevelCellIndex >= LevelCellCount)
	{
		return false;
	}

	const int32 LevelCellX = LevelCellIndex % LevelDimensions.X;
	const int32 LevelCellY = LevelCellIndex / LevelDimensions.X;
	const double CellMinX = LocalBounds.Min.X + static_cast<double>(LevelCellX) * LevelCellSizeCm;
	const double CellMinY = LocalBounds.Min.Y + static_cast<double>(LevelCellY) * LevelCellSizeCm;
	OutMinLocal = FVector2D(CellMinX, CellMinY);
	OutMaxLocal = FVector2D(
		FMath::Min(CellMinX + LevelCellSizeCm, static_cast<double>(LocalBounds.Max.X)),
		FMath::Min(CellMinY + LevelCellSizeCm, static_cast<double>(LocalBounds.Max.Y)));
	return true;
}

int32 FArcEcotopeSiteMetadata::MapCellIndexBetweenLevels(
	const int32 InIndex,
	const uint8 FromLevel,
	const uint8 ToLevel) const
{
	const FIntPoint FromDimensions = GetLevelDimensions(FromLevel);
	const FIntPoint ToDimensions = GetLevelDimensions(ToLevel);
	const int32 FromCellCount = GetLevelCellCount(FromLevel);
	const int32 ToCellCount = GetLevelCellCount(ToLevel);
	if (FromCellCount <= 0 || ToCellCount <= 0 || InIndex < 0 || InIndex >= FromCellCount)
	{
		return INDEX_NONE;
	}

	const int32 FromCellX = InIndex % FromDimensions.X;
	const int32 FromCellY = InIndex / FromDimensions.X;

	int32 ToCellX = FromCellX;
	int32 ToCellY = FromCellY;
	if (ToLevel > FromLevel)
	{
		const int32 Shift = static_cast<int32>(ToLevel) - static_cast<int32>(FromLevel);
		ToCellX = FromCellX >> Shift;
		ToCellY = FromCellY >> Shift;
	}
	else if (ToLevel < FromLevel)
	{
		const int32 Shift = static_cast<int32>(FromLevel) - static_cast<int32>(ToLevel);
		ToCellX = FromCellX << Shift;
		ToCellY = FromCellY << Shift;
	}

	if (ToCellX < 0 || ToCellY < 0 || ToCellX >= ToDimensions.X || ToCellY >= ToDimensions.Y)
	{
		return INDEX_NONE;
	}

	const int64 ToIndex = static_cast<int64>(ToCellX) + static_cast<int64>(ToCellY) * ToDimensions.X;
	if (ToIndex < 0 || ToIndex >= static_cast<int64>(ToCellCount))
	{
		return INDEX_NONE;
	}

	return static_cast<int32>(ToIndex);
}

double FArcEcotopeSiteMetadata::GetLevelCellAreaSquareCentimeters(
	const int32 LevelCellIndex,
	const uint8 Level) const
{
	FVector2D CellMinLocal = FVector2D::ZeroVector;
	FVector2D CellMaxLocal = FVector2D::ZeroVector;
	if (!TryGetLevelCellRect(LevelCellIndex, Level, CellMinLocal, CellMaxLocal))
	{
		return 0.0;
	}

	return FMath::Max(0.0, CellMaxLocal.X - CellMinLocal.X)
		* FMath::Max(0.0, CellMaxLocal.Y - CellMinLocal.Y);
}
