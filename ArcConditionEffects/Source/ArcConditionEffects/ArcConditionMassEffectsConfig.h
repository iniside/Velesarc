// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcConditionTypes.h"
#include "Engine/DataAsset.h"

#include "ArcConditionMassEffectsConfig.generated.h"

class UArcEffectDefinition;

USTRUCT(BlueprintType)
struct ARCCONDITIONEFFECTS_API FArcConditionMassEffectEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Condition")
	TObjectPtr<UArcEffectDefinition> ActivationEffect = nullptr;

	UPROPERTY(EditAnywhere, Category = "Condition")
	TObjectPtr<UArcEffectDefinition> BurstEffect = nullptr;

	UPROPERTY(EditAnywhere, Category = "Condition")
	TObjectPtr<UArcEffectDefinition> OverloadEffect = nullptr;
};

UCLASS(BlueprintType)
class ARCCONDITIONEFFECTS_API UArcConditionMassEffectsConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Conditions")
	TMap<EArcConditionType, FArcConditionMassEffectEntry> ConditionEffects;
};
