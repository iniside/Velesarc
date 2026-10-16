// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sampler/ArcForestSamplerSettings.h"
#include "Simulation/ArcEcotopeVolume.h"

#include "ArcEcotopeSamplerVolume.generated.h"

struct FArcForestSamplerInput;

UCLASS(BlueprintType)
class ARCECOTOPE_API AArcEcotopeSamplerVolume : public AArcEcotopeVolume
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sampler")
	FArcForestSamplerSettings SamplerSettings;

#if WITH_EDITOR
	bool FreezeSamplerInput(FArcForestSamplerInput& OutInput, FText& OutError) const;
#endif
};
