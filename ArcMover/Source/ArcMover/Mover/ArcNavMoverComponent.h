// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/NavMoverComponent.h"
#include "ArcNavMoverComponent.generated.h"

UCLASS(MinimalAPI, BlueprintType, meta = (BlueprintSpawnableComponent))
class UArcNavMoverComponent : public UNavMoverComponent
{
	GENERATED_BODY()

public:
	ARCMOVER_API virtual float GetMaxSpeedForNavMovement() const override;
};
