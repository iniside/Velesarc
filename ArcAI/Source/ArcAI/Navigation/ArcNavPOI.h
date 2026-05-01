// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "ArcNavPOI.generated.h"

class UBillboardComponent;

UCLASS(MinimalAPI, meta = (DisplayName = "Nav POI"))
class AArcNavPOI : public AActor
{
    GENERATED_BODY()

public:
    AArcNavPOI(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
    FGameplayTagContainer POITags;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation", meta = (ClampMin = "0.0", ForceUnits = "cm"))
    float MaxConnectionRange = 200000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
    TArray<TSoftObjectPtr<AArcNavPOI>> ManualConnections;

#if WITH_EDITORONLY_DATA
    UPROPERTY()
    TObjectPtr<UBillboardComponent> SpriteComponent;
#endif
};
