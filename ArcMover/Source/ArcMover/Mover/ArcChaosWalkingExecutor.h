// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ChaosMover/Character/Executors/ChaosCharacterWalkingExecutor.h"
#include "ArcChaosWalkingExecutor.generated.h"

UCLASS(MinimalAPI, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UArcChaosWalkingExecutor : public UChaosCharacterWalkingExecutor
{
	GENERATED_BODY()

public:
	ARCMOVER_API virtual void ExecuteMove_Async(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;
};
