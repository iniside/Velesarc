// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Assets/ArcTerrainBakedField.h"
#include "CoreMinimal.h"
#include "Terrain/ArcEcotopeInstanceRaster.h"

class AArcEcotopeVolume;
class UArcTerrainChannelSet;
struct FArcEcotopeSiteMetadata;

namespace ArcEcotopeWeightRaster
{
	bool BuildWeightRasters(
		const AArcEcotopeVolume& Volume,
		const FArcEcotopeSiteMetadata& Site,
		const UArcTerrainChannelSet* ChannelSet,
		FName CanopyChannel,
		FName LitterChannel,
		float MaskExpandCm,
		float MaskFalloffCm,
		float MaskFalloffExponent,
		TConstArrayView<FArcEcotopeInstanceFootprint> Footprints,
		TMap<FName, FArcTerrainBakedField>& OutLayers,
		TMap<FName, FName>& OutChannelMap,
		TArray<FName>& OutSuppressedChannels,
		FArcTerrainBakedField& OutUnionMask,
		FText& OutError);
}
