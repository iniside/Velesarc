// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcConditionTypes.h"
#include "Engine/DataAsset.h"
#include "GameplayEffect.h"

#include "ArcConditionEffectsConfig.generated.h"

USTRUCT(BlueprintType)
struct ARCCONDITIONEFFECTS_API FArcConditionEffectEntry
{
	GENERATED_BODY()

	/** Applied when condition activates. Must have AssetTag: ArcCondition.Active.{Name} for removal. */
	UPROPERTY(EditAnywhere, Category = "Condition")
	TSubclassOf<UGameplayEffect> ActivationEffect;

	/** Applied once at saturation=100 (Break). Instant, no tag needed. */
	UPROPERTY(EditAnywhere, Category = "Condition")
	TSubclassOf<UGameplayEffect> BurstEffect;

	/** Applied when overload begins. Must have AssetTag: ArcCondition.Overload.{Name} for removal. */
	UPROPERTY(EditAnywhere, Category = "Condition")
	TSubclassOf<UGameplayEffect> OverloadEffect;
};

UCLASS(BlueprintType)
class ARCCONDITIONEFFECTS_API UArcConditionEffectsConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Conditions")
	TMap<EArcConditionType, FArcConditionEffectEntry> ConditionEffects;
};
