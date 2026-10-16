// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ChaosMover/Character/ChaosCharacterMoverComponent.h"
#include "ChaosMover/Character/Settings/SharedChaosCharacterMovementSettings.h"
#include "ArcChaosCharacterMoverComponent.generated.h"

UCLASS(MinimalAPI, BlueprintType)
class UArcChaosCharacterMovementSettings : public USharedChaosCharacterMovementSettings
{
	GENERATED_BODY()

public:
	ARCMOVER_API UArcChaosCharacterMovementSettings(const FObjectInitializer& ObjectInitializer);
};

UCLASS(MinimalAPI, BlueprintType, meta = (BlueprintSpawnableComponent))
class UArcChaosCharacterMoverComponent : public UChaosCharacterMoverComponent
{
	GENERATED_BODY()

public:
	ARCMOVER_API UArcChaosCharacterMoverComponent();

	ARCMOVER_API virtual void InitializeComponent() override;

	ARCMOVER_API FMovementModifierHandle QueueSpeedScaleMovementModifier(const FGuid& SourceId, float SpeedPercent);
};
