// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sampler/ArcForestSamplerSettings.h"
#include "Simulation/ArcEcotopeFrozenSiteSource.h"
#include "Simulation/ArcForestBootstrap.h"

struct ARCECOTOPE_API FArcForestSamplerFrozenBand
{
	float MinHeightCm = 0.0f;
	float MaxHeightCm = 0.0f;
	int32 BandIndex = INDEX_NONE;
};

struct ARCECOTOPE_API FArcForestSamplerFrozenBands
{
	FGuid SpeciesId;
	TArray<FArcForestSamplerFrozenBand> Bands;
};

struct ARCECOTOPE_API FArcForestSamplerInput
{
	FGuid VolumeId;
	int32 Seed = 0;
	int32 ChunkBaseCellSpan = 0;
	uint64 SettingsHash = 0;
	double MaximumCrownOccupancy = 1.3;
	double UnderstoryShade = 1.0;
	FArcForestSamplerSettings Settings;
	FArcEcotopeFrozenSiteSourcePtr SiteSource;
	TArray<FArcForestFrozenSpeciesSetup> SpeciesSetups;
	TArray<FArcForestSamplerFrozenBands> SpeciesBands;
};
