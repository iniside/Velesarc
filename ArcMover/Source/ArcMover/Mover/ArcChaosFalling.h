// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ChaosMover/Character/Modes/ChaosFallingMode.h"
#include "ChaosMover/Character/Sources/ChaosCharacterAirSource.h"
#include "ArcChaosFalling.generated.h"

UCLASS(MinimalAPI, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UArcChaosAirSource : public UChaosCharacterAirSource
{
	GENERATED_BODY()

public:
	ARCMOVER_API UArcChaosAirSource(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(Category = Mover, EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ForceUnits = "deg/s"))
	float AirTurningRateLimit = 300.0f;

	ARCMOVER_API virtual void GenerateMove_Async(
		const FMoverTickStartData& StartState,
		const FMoverTimeStep& TimeStep,
		UMoverBlackboard* SimBlackboard,
		FProposedMove& OutProposedMove) override;

	ARCMOVER_API virtual float GetMaxSpeed() const override;

private:
	float StepSpeedScale = 1.0f;
};

UCLASS(MinimalAPI, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UArcChaosFallingMode : public UChaosFallingMode
{
	GENERATED_BODY()

public:
	ARCMOVER_API UArcChaosFallingMode(const FObjectInitializer& ObjectInitializer);
};
