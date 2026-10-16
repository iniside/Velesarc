// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FArcTownFieldSettings;
struct FArcTownSiteSnapshot;

/** Built once per session on a worker, never per year: the snapshot and the settings are the whole input. */
struct FArcTownSiteFields
{
	FIntPoint Dims = FIntPoint::ZeroValue;

	int32 MarginCells = 0;

	double SpacingCm = 0.0;

	TArray<float> Slope;

	TArray<float> DistanceToWaterCm;

	TArray<float> HeightAboveChannel;

	TArray<uint8> bBuildable;
};

namespace ArcTownSiteFields
{
	ARCTOWN_API bool BuildSiteFields(
		const FArcTownSiteSnapshot& Site,
		const FArcTownFieldSettings& Settings,
		FArcTownSiteFields& OutFields,
		FText& OutError);
}
