// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AArcTownVolume;
struct FArcTownSiteSnapshot;

namespace ArcTownSiteSampler
{
	ARCTOWNEDITOR_API bool SampleSite(AArcTownVolume& Volume, FArcTownSiteSnapshot& OutSnapshot, FText& OutError);
}
