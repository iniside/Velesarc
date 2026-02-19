// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcWeatherSubsystem.h"
#include "GameFramework/Actor.h"
#include "ArcClimateRegionVolume.generated.h"

class UBoxComponent;

// Editor-placed volume that registers its base climate into the global weather grid on BeginPlay
UCLASS()
class ARCWEATHER_API AArcClimateRegionVolume : public AActor
{
	GENERATED_BODY()

public:
	AArcClimateRegionVolume();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climate")
	FArcClimateParams BaseClimate;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climate")
	TObjectPtr<UBoxComponent> BoxComponent;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
