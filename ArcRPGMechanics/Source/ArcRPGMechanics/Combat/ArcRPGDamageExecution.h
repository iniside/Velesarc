// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Combat/ArcRPGAttackPath.h"
#include "ArcMassMechanics/Combat/ArcDamageExecutionBase.h"
#include "ArcRPGDamageExecution.generated.h"

class UArcRPGHitZoneConfig;
class UArcRPGArmorConfig;

UENUM()
enum class EArcRPGDamageValueTiming : uint8
{
	AtApplication UMETA(ToolTip = "Every attacker and target value (outgoing and incoming bonuses with their caps, damage reduction, global Armor and Resistance) is read once when a Duration or Infinite effect is applied; every periodic tick reuses them."),
	EveryExecution UMETA(ToolTip = "Every attacker and target value is read at every execution, periodic ticks included. Source item values (weapon Damage and Scale, skill power, attribute factor) stay the ones captured when the spec was built.")
};

USTRUCT()
struct ARCRPGMECHANICS_API FArcRPGDamageInputs
{
	GENERATED_BODY()

	UPROPERTY()
	float SpellDamagePct = 0.f;

	UPROPERTY()
	float OutgoingBonusPercent = 0.f;

	UPROPERTY()
	float MaxOutgoingBonusPercent = 0.f;

	UPROPERTY()
	float OutgoingReductionPercent = 0.f;

	UPROPERTY()
	float MaxOutgoingReductionPercent = 0.f;

	UPROPERTY()
	float OutgoingBonus = 0.f;

	UPROPERTY()
	float OutgoingReduction = 0.f;

	UPROPERTY()
	float IncomingBonus = 0.f;

	UPROPERTY()
	float IncomingBonusPercent = 0.f;

	UPROPERTY()
	float MaxIncomingBonusPercent = 0.f;

	UPROPERTY()
	float DamageReduction = 0.f;

	UPROPERTY()
	float Armor = 0.f;

	UPROPERTY()
	float Resistance = 0.f;
};

USTRUCT(BlueprintType, meta = (DisplayName = "RPG Damage Execution", ToolTip = "One hit of the effect's ArcEffect.Damage type, in combat.yaml pipeline order, written to Damage. Base: on the weapon path (ArcRPG.AttackClass.Weapon) the source item's Weapon Damage and Scale, else the authored Base and Scale, x Weapon Multiplier. Then Base * (1 + Scale * skill power) x attribute factor, both from the source item's Skill Scaling fragment and captured at spec build; x (1 + Spell Damage % on the spell path + outgoing %); x If Done; x crit (only with the Crit attribute set); flat outgoing; target incoming; mitigation. Crit, zone and If on applied hits only. A missing config asset skips its step."))
struct ARCRPGMECHANICS_API FArcRPGDamageExecution : public FArcDamageExecutionBase
{
	GENERATED_BODY()

	FArcRPGDamageExecution();

	UPROPERTY(EditAnywhere, Category = "Damage", meta = (ToolTip = "Hit zone roll. Without it no zone is rolled and no armor piece is struck."))
	TObjectPtr<UArcRPGHitZoneConfig> HitZoneConfig = nullptr;

	UPROPERTY(EditAnywhere, Category = "Damage", meta = (ToolTip = "Armor step. Without it no armor applies."))
	TObjectPtr<UArcRPGArmorConfig> ArmorConfig = nullptr;

	UPROPERTY(EditAnywhere, Category = "Damage", meta = (ClampMin = "0", ToolTip = "Attack multiplier m: multiplies the base (weapon Damage or the authored Base)."))
	float WeaponMultiplier = 1.f;

	UPROPERTY(EditAnywhere, Category = "Damage", meta = (ToolTip = "Base when there is no captured weapon: spells, attacks with neither attack path tag, and weapon attacks whose source item has no Weapon Damage."))
	float Base = 0.f;

	UPROPERTY(EditAnywhere, Category = "Damage", meta = (ToolTip = "Scale paired with Base when there is no captured weapon."))
	float Scale = 0.f;

	UPROPERTY(EditAnywhere, Category = "Damage", meta = (ToolTip = "When periodic ticks read attacker and target values. Applied hits always read them on application. Source item values (weapon Damage and Scale, skill power, attribute factor) are always the ones captured when the spec was built."))
	EArcRPGDamageValueTiming ValueTiming = EArcRPGDamageValueTiming::AtApplication;

	virtual void GatherCaptures(TArray<FArcAttributeCaptureDef>& OutCaptures, TArray<FArcExtraCaptureDef>& OutExtras) const override;

	virtual void CaptureApplicationState(const FArcEffectExecutionContext& Context, FInstancedStruct& OutState) const override;

	virtual void Execute(const FArcEffectExecutionContext& Context, TArray<FArcEffectModifierOutput>& OutModifiers) const override;

#if WITH_EDITOR
	virtual void GatherDataWarnings(TArray<FText>& OutWarnings) const override;
#endif

private:
	void GatherBase(const FArcEffectExecutionContext& Context, EArcRPGAttackPath Path, float& OutBase, float& OutScale) const;

	void ReadInputs(const FArcEffectExecutionContext& Context, const FArcEvaluateParameters& HitParams, EArcRPGAttackPath Path,
		FArcRPGDamageInputs& OutInputs) const;

	float ResolveSourceValue(const FArcEffectExecutionContext& Context, const FArcRPGDamageInputs& Inputs, float InBase, float InScale,
		float CritMultiplier, float IfDoneMultiplier, float IfDoneFlat) const;
};
