// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovementModifier.h"

#include "ArcChaosMovementModifiers.generated.h"

namespace ArcMover
{
	ARCMOVER_API float ComputeSpeedScale(const FMovementModifierGroup& Modifiers);
}

USTRUCT(BlueprintType)
struct ARCMOVER_API FArcChaosMovementModifier_SpeedScale : public FMovementModifierBase
{
	GENERATED_BODY()

	FArcChaosMovementModifier_SpeedScale();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArcMover")
	FGuid SourceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArcMover", meta = (ClampMin = "0"))
	float SpeedPercent = 100.0f;

	virtual FMovementModifierBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual bool Matches(const FMovementModifierBase* Other) const override;
};

template<>
struct TStructOpsTypeTraits<FArcChaosMovementModifier_SpeedScale> : public TStructOpsTypeTraitsBase2<FArcChaosMovementModifier_SpeedScale>
{
	enum
	{
		WithCopy = true
	};
};
