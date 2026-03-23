// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystem/ArcAttributeSet.h"
#include "AbilitySystem/ArcAttributesTypes.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"

#include "ArcDamageAttributeSet.generated.h"

class UArcDamageResistanceMappingAsset;
/**
 * Tag-filtered damage attributes. GE modifiers use SourceTags.RequireTags with DamageType.* tags
 * to scope bonuses/resistances to specific damage types. Untagged modifiers apply to all types.
 *
 * When DamageResistance changes, the resistance bridge evaluates per-tag resistance values
 * and writes them into matching condition fragment State.Resistance fields (configured via
 * UArcDamageResistanceMappingAsset).
 */
UCLASS()
class ARCDAMAGESYSTEM_API UArcDamageAttributeSet : public UArcAttributeSet
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayAttributeData OutgoingDamageBonus;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayAttributeData OutgoingDamageBonusPercent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayAttributeData MaxOutgoingDamageBonusPercent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayAttributeData OutgoingDamageReduction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayAttributeData OutgoingDamageReductionPercent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayAttributeData MaxOutgoingDamageReductionPercent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayAttributeData IncomingDamageBonus;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayAttributeData IncomingDamageBonusPercent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayAttributeData MaxIncomingDamageBonusPercent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayAttributeData DamageResistance;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayAttributeData MaxDamageResistance;

public:
	ARC_ATTRIBUTE_ACCESSORS(UArcDamageAttributeSet, OutgoingDamageBonus);
	ARC_ATTRIBUTE_ACCESSORS(UArcDamageAttributeSet, OutgoingDamageBonusPercent);
	ARC_ATTRIBUTE_ACCESSORS(UArcDamageAttributeSet, MaxOutgoingDamageBonusPercent);
	ARC_ATTRIBUTE_ACCESSORS(UArcDamageAttributeSet, OutgoingDamageReduction);
	ARC_ATTRIBUTE_ACCESSORS(UArcDamageAttributeSet, OutgoingDamageReductionPercent);
	ARC_ATTRIBUTE_ACCESSORS(UArcDamageAttributeSet, MaxOutgoingDamageReductionPercent);
	ARC_ATTRIBUTE_ACCESSORS(UArcDamageAttributeSet, IncomingDamageBonus);
	ARC_ATTRIBUTE_ACCESSORS(UArcDamageAttributeSet, IncomingDamageBonusPercent);
	ARC_ATTRIBUTE_ACCESSORS(UArcDamageAttributeSet, MaxIncomingDamageBonusPercent);
	ARC_ATTRIBUTE_ACCESSORS(UArcDamageAttributeSet, DamageResistance);
	ARC_ATTRIBUTE_ACCESSORS(UArcDamageAttributeSet, MaxDamageResistance);

	ARC_ATTRIBUTE_CAPTURE_SOURCE(UArcDamageAttributeSet, OutgoingDamageBonus);
	ARC_ATTRIBUTE_CAPTURE_SOURCE(UArcDamageAttributeSet, OutgoingDamageBonusPercent);
	ARC_ATTRIBUTE_CAPTURE_SOURCE(UArcDamageAttributeSet, OutgoingDamageReduction);
	ARC_ATTRIBUTE_CAPTURE_SOURCE(UArcDamageAttributeSet, OutgoingDamageReductionPercent);

	ARC_ATTRIBUTE_CAPTURE_TARGET(UArcDamageAttributeSet, IncomingDamageBonus);
	ARC_ATTRIBUTE_CAPTURE_TARGET(UArcDamageAttributeSet, IncomingDamageBonusPercent);
	ARC_ATTRIBUTE_CAPTURE_TARGET(UArcDamageAttributeSet, DamageResistance);

	ARC_DEFINE_PRE_ATTRIBUTE_HANDLER(OutgoingDamageBonusPercent);
	ARC_DEFINE_PRE_ATTRIBUTE_HANDLER(OutgoingDamageReductionPercent);
	ARC_DEFINE_PRE_ATTRIBUTE_HANDLER(IncomingDamageBonusPercent);
	ARC_DEFINE_PRE_ATTRIBUTE_HANDLER(DamageResistance);

	ARC_DEFINE_POST_ATTRIBUTE_HANDLER(DamageResistance);

	UArcDamageAttributeSet();

private:
	/** Cached mapping asset. Resolved from ASC AbilitySystemData on first use. */
	UPROPERTY(Transient)
	TObjectPtr<UArcDamageResistanceMappingAsset> CachedMappingAsset;

	/** Cached capture spec for DamageResistance, created once. */
	TOptional<FGameplayEffectAttributeCaptureSpec> CachedResistanceCaptureSpec;

	/** Syncs per-tag DamageResistance values into condition fragment State.Resistance fields. */
	void SyncConditionResistances();
};
