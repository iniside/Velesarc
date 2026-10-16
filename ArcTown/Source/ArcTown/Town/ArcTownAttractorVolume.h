// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Misc/DataValidation.h"

#include "ArcTownAttractorVolume.generated.h"

class UBoxComponent;

UCLASS(BlueprintType)
class ARCTOWN_API AArcTownAttractorVolume : public AActor
{
	GENERATED_BODY()

public:
	AArcTownAttractorVolume();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attractor")
	TObjectPtr<UBoxComponent> VolumeBounds;

	UPROPERTY(EditAnywhere, Category = "Attractor", meta = (Categories = "Town.District"))
	FGameplayTagContainer DistrictTags;

	UPROPERTY(EditAnywhere, Category = "Attractor", meta = (ClampMin = "0.0", Units = "cm"))
	double InfluenceRadius = 3000.0;

	/** Weight of this volume against emergent building clusters, whose strength is 1. */
	UPROPERTY(EditAnywhere, Category = "Attractor", meta = (ClampMin = "0.0"))
	float Strength = 4.0f;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
