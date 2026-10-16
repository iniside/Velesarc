// Copyright Lukasz Baran. All Rights Reserved.

#include "Foliage/ArcEcotopeFoliageFieldPayload.h"

#include "Math/Float16Color.h"

bool FArcEcotopeFoliageFieldPayload::IsValid() const
{
	if (Dimensions.X <= 0 || Dimensions.Y <= 0
		|| Dimensions.X > ArcEcotopeFoliageFieldLimits::MaxFieldTexels
		|| Dimensions.Y > ArcEcotopeFoliageFieldLimits::MaxFieldTexels)
	{
		return false;
	}

	if (!(TexelSizeCm > 0.0) || !FMath::IsFinite(TexelSizeCm))
	{
		return false;
	}

	if (PayloadRev != ArcEcotopeFoliageFieldLimits::FieldPayloadRev)
	{
		return false;
	}

	const int64 ExpectedBytes =
		static_cast<int64>(Dimensions.X) * Dimensions.Y * sizeof(FFloat16Color);
	return ExpectedBytes > 0 && ExpectedBytes <= MAX_int32 && Texels.Num() == ExpectedBytes;
}

void FArcEcotopeFoliageFieldPayload::Reset()
{
	Dimensions = FIntPoint::ZeroValue;
	TexelSizeCm = 0.0;
	OriginLocal = FVector2D::ZeroVector;
	SourceSiteSnapshotHash = 0;
	SourceStateChecksum = 0;
	PayloadRev = 0;
	Texels.Empty();
}
