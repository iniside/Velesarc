// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE::ArcForest
{
	inline constexpr float DisturbanceNoiseFrequencyPerCm = 0.0002f;
	inline constexpr int32 DisturbanceNoiseOctaves = 3;
	inline constexpr float DisturbanceNoiseLacunarity = 2.0f;

	ARCECOTOPE_API float SampleDisturbanceNoise01(FVector2f LocalPos, uint32 NoiseSeed);
}
