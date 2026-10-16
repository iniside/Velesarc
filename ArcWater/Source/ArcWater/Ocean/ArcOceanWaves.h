// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"

#include "ArcOceanWaves.generated.h"

namespace ArcOcean
{
	inline constexpr int32 MaxWaves = 16;

	inline constexpr uint32 ShallowBandFlag = 1u;

	// Newton needs |det J| > FrameEpsilon (1e-4) and det J >= (1 - sum of per-wave Steepness)^2, so the sum stops at 1 - sqrt(1e-4).
	inline constexpr float ARC_OCEAN_MAX_STEEPNESS_SUM = 1.0f - 0.01f;
}

struct FArcGerstnerWave
{
	FVector2f Direction = FVector2f::ZeroVector;
	float Wavelength = 0.0f;
	float Amplitude = 0.0f;
	float Steepness = 0.0f;
	float Phase = 0.0f;
	uint32 BandFlags = 0;
	float Padding = 0.0f;
};

static_assert(sizeof(FArcGerstnerWave) == 2 * sizeof(FVector4f), "FArcGerstnerWave must pack into exactly two float4 shader registers.");

USTRUCT(BlueprintType)
struct ARCWATER_API FArcOceanShallowBandSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean|Shallow Band", meta = (ClampMin = "0", ClampMax = "8"))
	int32 NumWaves = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean|Shallow Band", meta = (ClampMin = "1.0", Units = "Centimeters"))
	float LongestWavelengthCm = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean|Shallow Band", meta = (ClampMin = "0.125", ClampMax = "0.875"))
	float WavelengthRatio = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean|Shallow Band", meta = (ClampMin = "0.0009765625", ClampMax = "0.125"))
	float WavelengthToAmplitude = 0.015f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean|Shallow Band", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "Degrees"))
	float SpreadDeg = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean|Shallow Band", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Steepness = 0.25f;
};

USTRUCT(BlueprintType)
struct ARCWATER_API FArcOceanWaveSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean|Waves")
	FVector2D WindDirection = FVector2D(1.0, 0.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean|Waves", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "Degrees"))
	float SpreadDeg = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean|Waves", meta = (ClampMin = "1.0", Units = "Centimeters"))
	float LongestWavelengthCm = 25000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean|Waves", meta = (ClampMin = "0.125", ClampMax = "0.875"))
	float WavelengthRatio = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean|Waves", meta = (ClampMin = "1", ClampMax = "16"))
	int32 NumWaves = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean|Waves", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Steepness = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean|Waves", meta = (ClampMin = "0.0009765625", ClampMax = "0.125"))
	float WavelengthToAmplitude = 0.0047f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean|Shallow Band")
	FArcOceanShallowBandSettings ShallowBand;
};

struct FArcOceanBandLimits
{
	int32 SwellNumWaves = 0;
	float SwellLongestWavelengthCm = 0.0f;
	float SwellWavelengthRatio = 0.0f;
	float SwellSteepness = 0.0f;
	int32 ShallowNumWaves = 0;
	float ShallowCeilingCm = 0.0f;
	float ShallowLongestWavelengthCm = 0.0f;
	float ShallowSteepness = 0.0f;
};

namespace ArcOceanWaves
{
	ARCWATER_API void ResolveBandLimits(const FArcOceanWaveSettings& InSettings, FArcOceanBandLimits& OutLimits);

	ARCWATER_API void BuildWaveSet(const FArcOceanWaveSettings& InSettings, TArray<FArcGerstnerWave>& OutWaves);

	ARCWATER_API void SolveSurfaceAt(
		TConstArrayView<FArcGerstnerWave> InWaves,
		FVector2f ObservedXY,
		float TimeSeconds,
		float EffectiveCellSize,
		float DepthCm,
		FVector2f InShoreward,
		float& OutHeightZ,
		FVector3f& OutNormal,
		float& OutVerticalSpeed,
		float& OutAmplitudeSumCm);
}

namespace ArcOcean
{
	struct FArcOceanWaveFields
	{
		FVector4f Waves[MaxWaves * 2];
		int32 WaveCount = 0;
	};

	ARCWATER_API void PackWaveParams(const FArcOceanWaveSettings& InSettings, FArcOceanWaveFields& OutFields);
}
