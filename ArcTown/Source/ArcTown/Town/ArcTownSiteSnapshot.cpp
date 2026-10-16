// Copyright Lukasz Baran. All Rights Reserved.

#include "Town/ArcTownSiteSnapshot.h"

#include "Simulation/ArcTownDeterministic.h"

namespace ArcTownSiteSnapshotPrivate
{
	constexpr uint64 SiteSnapshotHashKey = 0x5D3F9A17C4E2B86Bull;

	constexpr double QuantaPerCentimetre = 1000.0;

	constexpr double QuantaPerUnitAxis = 1000000.0;

	constexpr double MaxQuantizedMagnitude = 9.0e15;

	void AddQuantized(ArcTownDeterministic::FKeyedHash64& Hash, const double Value, const double QuantaPerUnit)
	{
		const double Scaled = FMath::IsFinite(Value) ? Value * QuantaPerUnit : 0.0;
		const double Clamped = FMath::Clamp(Scaled, -MaxQuantizedMagnitude, MaxQuantizedMagnitude);
		Hash.AddInt64(static_cast<int64>(FMath::RoundToDouble(Clamped)));
	}

	void AddFloatBits(ArcTownDeterministic::FKeyedHash64& Hash, float Value)
	{
		if (Value == 0.0f)
		{
			Value = 0.0f;
		}

		uint32 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		Hash.AddUInt32(Bits);
	}
}

FIntPoint FArcTownSiteSnapshot::GetSiteDims() const
{
	const int64 MarginSpan = 2 * static_cast<int64>(MarginCells);
	const int64 SiteWidth = static_cast<int64>(GridDims.X) - MarginSpan;
	const int64 SiteHeight = static_cast<int64>(GridDims.Y) - MarginSpan;
	return FIntPoint(
		static_cast<int32>(FMath::Clamp<int64>(SiteWidth, MIN_int32, MAX_int32)),
		static_cast<int32>(FMath::Clamp<int64>(SiteHeight, MIN_int32, MAX_int32)));
}

FVector2D FArcTownSiteSnapshot::GetSiteOriginLocal() const
{
	const double Inset = static_cast<double>(MarginCells) * SampleSpacing;
	return FVector2D(OriginLocal.X + Inset, OriginLocal.Y + Inset);
}

int32 FArcTownSiteSnapshot::SnapshotIndex(const int32 SiteX, const int32 SiteY) const
{
	const int64 Index = (static_cast<int64>(SiteY) + static_cast<int64>(MarginCells)) * static_cast<int64>(GridDims.X)
		+ (static_cast<int64>(SiteX) + static_cast<int64>(MarginCells));
	return static_cast<int32>(FMath::Clamp<int64>(Index, MIN_int32, MAX_int32));
}

uint64 FArcTownSiteSnapshot::ComputeHash() const
{
	using namespace ArcTownSiteSnapshotPrivate;

	ArcTownDeterministic::FKeyedHash64 Hash(SiteSnapshotHashKey);
	Hash.AddGuid(TerrainGuid);

	const FVector Translation = TerrainToWorld.GetTranslation();
	AddQuantized(Hash, Translation.X, QuantaPerCentimetre);
	AddQuantized(Hash, Translation.Y, QuantaPerCentimetre);
	AddQuantized(Hash, Translation.Z, QuantaPerCentimetre);

	const FQuat Rotation = TerrainToWorld.GetRotation();
	AddQuantized(Hash, Rotation.X, QuantaPerUnitAxis);
	AddQuantized(Hash, Rotation.Y, QuantaPerUnitAxis);
	AddQuantized(Hash, Rotation.Z, QuantaPerUnitAxis);
	AddQuantized(Hash, Rotation.W, QuantaPerUnitAxis);

	const FVector Scale = TerrainToWorld.GetScale3D();
	AddQuantized(Hash, Scale.X, QuantaPerUnitAxis);
	AddQuantized(Hash, Scale.Y, QuantaPerUnitAxis);
	AddQuantized(Hash, Scale.Z, QuantaPerUnitAxis);

	Hash.AddInt32(GridDims.X);
	Hash.AddInt32(GridDims.Y);
	AddQuantized(Hash, SampleSpacing, QuantaPerCentimetre);
	Hash.AddInt32(MarginCells);
	AddFloatBits(Hash, RiversChannelThreshold);
	AddQuantized(Hash, OriginLocal.X, QuantaPerCentimetre);
	AddQuantized(Hash, OriginLocal.Y, QuantaPerCentimetre);

	Hash.AddInt32(Heights.Num());
	for (const float Height : Heights)
	{
		AddFloatBits(Hash, Height);
	}

	Hash.AddInt32(Normals.Num());
	for (const FVector3f& Normal : Normals)
	{
		AddFloatBits(Hash, Normal.X);
		AddFloatBits(Hash, Normal.Y);
		AddFloatBits(Hash, Normal.Z);
	}

	Hash.AddInt32(ChannelMask.Num());
	for (const uint8 Flag : ChannelMask)
	{
		Hash.AddUInt8(Flag);
	}

	return Hash.Finalize();
}

bool FArcTownSiteSnapshot::IsEmpty() const
{
	return GridDims == FIntPoint::ZeroValue && Heights.Num() == 0 && Normals.Num() == 0 && ChannelMask.Num() == 0;
}

int64 FArcTownSiteSnapshot::GetSampleCount() const
{
	if (GridDims.X <= 0 || GridDims.Y <= 0)
	{
		return 0;
	}

	return static_cast<int64>(GridDims.X) * static_cast<int64>(GridDims.Y);
}

bool FArcTownSiteSnapshot::IsValid() const
{
	const int64 SampleCount = GetSampleCount();
	if (SampleCount <= 0 || SampleCount > MaxSampleCount)
	{
		return false;
	}

	if (static_cast<int64>(Heights.Num()) != SampleCount)
	{
		return false;
	}

	if (Normals.Num() != 0 && Normals.Num() != Heights.Num())
	{
		return false;
	}

	if (static_cast<int64>(ChannelMask.Num()) != SampleCount)
	{
		return false;
	}

	const int64 MarginSpan = 2 * static_cast<int64>(MarginCells);
	if (MarginCells < MinMarginCells
		|| static_cast<int64>(GridDims.X) <= MarginSpan
		|| static_cast<int64>(GridDims.Y) <= MarginSpan)
	{
		return false;
	}

	if (!FMath::IsFinite(RiversChannelThreshold))
	{
		return false;
	}

	if (!FMath::IsFinite(SampleSpacing) || SampleSpacing <= 0.0)
	{
		return false;
	}

	if (!TerrainGuid.IsValid())
	{
		return false;
	}

	// FTransform::IsValid() is only the NaN and rotation-normalization test; it never looks at scale,
	// and a zero-scaled frame is not invertible for a local->world consumer.
	if (!TerrainToWorld.IsValid() || TerrainToWorld.GetScale3D().IsNearlyZero())
	{
		return false;
	}

	if (!FMath::IsFinite(OriginLocal.X) || !FMath::IsFinite(OriginLocal.Y))
	{
		return false;
	}

	for (const float Height : Heights)
	{
		if (!FMath::IsFinite(Height))
		{
			return false;
		}
	}

	for (const FVector3f& Normal : Normals)
	{
		if (!FMath::IsFinite(Normal.X) || !FMath::IsFinite(Normal.Y) || !FMath::IsFinite(Normal.Z)
			|| Normal.IsNearlyZero())
		{
			return false;
		}
	}

	return true;
}

bool ArcTownSiteSampling::IsInsideSampledSite(const FArcTownSiteSnapshot& Snapshot, const FVector2D& WorldXY)
{
	const FVector LocalPos = Snapshot.TerrainToWorld.InverseTransformPosition(FVector(WorldXY.X, WorldXY.Y, 0.0));
	const FIntPoint SiteDims = Snapshot.GetSiteDims();
	const FVector2D SiteOrigin = Snapshot.GetSiteOriginLocal();
	const double GridX = (LocalPos.X - SiteOrigin.X) / Snapshot.SampleSpacing;
	const double GridY = (LocalPos.Y - SiteOrigin.Y) / Snapshot.SampleSpacing;
	return FMath::IsFinite(GridX)
		&& FMath::IsFinite(GridY)
		&& GridX >= 0.0
		&& GridY >= 0.0
		&& GridX <= static_cast<double>(SiteDims.X - 1)
		&& GridY <= static_cast<double>(SiteDims.Y - 1);
}

bool ArcTownSiteSampling::ClampIntoSampledSite(const FArcTownSiteSnapshot& Snapshot, FVector2D& InOutWorldXY)
{
	const FVector LocalPos =
		Snapshot.TerrainToWorld.InverseTransformPosition(FVector(InOutWorldXY.X, InOutWorldXY.Y, 0.0));
	const FIntPoint SiteDims = Snapshot.GetSiteDims();
	const FVector2D SiteOrigin = Snapshot.GetSiteOriginLocal();
	const int32 MaxX = SiteDims.X - 1;
	const int32 MaxY = SiteDims.Y - 1;
	if (MaxX < 0 || MaxY < 0)
	{
		return false;
	}

	const double GridX = (LocalPos.X - SiteOrigin.X) / Snapshot.SampleSpacing;
	const double GridY = (LocalPos.Y - SiteOrigin.Y) / Snapshot.SampleSpacing;
	if (!FMath::IsFinite(GridX) || !FMath::IsFinite(GridY))
	{
		return false;
	}

	const double ClampedLocalX =
		SiteOrigin.X + FMath::Clamp(GridX, 0.0, static_cast<double>(MaxX)) * Snapshot.SampleSpacing;
	const double ClampedLocalY =
		SiteOrigin.Y + FMath::Clamp(GridY, 0.0, static_cast<double>(MaxY)) * Snapshot.SampleSpacing;
	const FVector ClampedWorld =
		Snapshot.TerrainToWorld.TransformPosition(FVector(ClampedLocalX, ClampedLocalY, 0.0));
	InOutWorldXY = FVector2D(ClampedWorld.X, ClampedWorld.Y);
	return true;
}

bool ArcTownSiteSampling::SampleGroundZ(const FArcTownSiteSnapshot& Snapshot, const FVector2D& WorldXY, double& OutWorldZ)
{
	const FVector LocalPos = Snapshot.TerrainToWorld.InverseTransformPosition(FVector(WorldXY.X, WorldXY.Y, 0.0));
	const FIntPoint SiteDims = Snapshot.GetSiteDims();
	const FVector2D SiteOrigin = Snapshot.GetSiteOriginLocal();
	const int32 MaxX = SiteDims.X - 1;
	const int32 MaxY = SiteDims.Y - 1;
	if (MaxX < 0 || MaxY < 0)
	{
		return false;
	}

	const double GridX = FMath::Clamp(
		(LocalPos.X - SiteOrigin.X) / Snapshot.SampleSpacing, 0.0, static_cast<double>(MaxX));
	const double GridY = FMath::Clamp(
		(LocalPos.Y - SiteOrigin.Y) / Snapshot.SampleSpacing, 0.0, static_cast<double>(MaxY));
	if (!FMath::IsFinite(GridX) || !FMath::IsFinite(GridY))
	{
		return false;
	}

	const int32 X0 = FMath::Clamp(FMath::FloorToInt32(GridX), 0, FMath::Max(MaxX - 1, 0));
	const int32 Y0 = FMath::Clamp(FMath::FloorToInt32(GridY), 0, FMath::Max(MaxY - 1, 0));
	const int32 X1 = FMath::Min(X0 + 1, MaxX);
	const int32 Y1 = FMath::Min(Y0 + 1, MaxY);
	const double FracX = GridX - static_cast<double>(X0);
	const double FracY = GridY - static_cast<double>(Y0);

	const double H00 = Snapshot.Heights[Snapshot.SnapshotIndex(X0, Y0)];
	const double H10 = Snapshot.Heights[Snapshot.SnapshotIndex(X1, Y0)];
	const double H01 = Snapshot.Heights[Snapshot.SnapshotIndex(X0, Y1)];
	const double H11 = Snapshot.Heights[Snapshot.SnapshotIndex(X1, Y1)];
	const double Height = FMath::Lerp(FMath::Lerp(H00, H10, FracX), FMath::Lerp(H01, H11, FracX), FracY);

	OutWorldZ = Snapshot.TerrainToWorld.TransformPosition(FVector(LocalPos.X, LocalPos.Y, Height)).Z;
	return FMath::IsFinite(OutWorldZ);
}
