// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "ArcRPGDerivedValueHandlers.generated.h"

UENUM(BlueprintType)
enum class EArcRPGChanceTermShape : uint8
{
	PerPoint UMETA(ToolTip = "Coefficient * input value."),
	PerModifier UMETA(ToolTip = "Coefficient * attribute modifier floor((input - 10) / 2) (D-08).")
};

USTRUCT(BlueprintType)
struct ARCRPGMECHANICS_API FArcRPGChanceTerm
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Term", meta = (ToolTip = "Attribute on the same entity. A change recomputes the value; a missing attribute adds nothing."))
	FArcAttributeRef Input;

	UPROPERTY(EditAnywhere, Category = "Term")
	EArcRPGChanceTermShape Shape = EArcRPGChanceTermShape::PerPoint;

	UPROPERTY(EditAnywhere, Category = "Term", meta = (ToolTip = "Value per point or per modifier step; percentages and chances as fractions (12% = 0.12). Negative lowers the value."))
	float Coefficient = 0.f;
};

USTRUCT(meta = (DisplayName = "RPG Linear", ToolTip = "Assign to a derived attribute. Writes the base value Base + sum of Terms (Coefficient * Input per point, or Coefficient * modifier(Input) per modifier step); effect modifiers still apply on top.\nDesign values: ThresholdEntropy 50 + WIL Per Point 5; CarryKg 50 + STR Per Point 5; AttackSpeedPct 0 + DEX Per Modifier 0.06; MovementSpeedPct 0 + AGI Per Modifier 0.06; DetectionRadius 10 + PER Per Point 1; HealingBonusPct 0 + WIS Per Modifier 0.12; FoodDrinkEfficiency 1 + WIS Per Modifier 0.08; CompanionEffectivenessPct 0 + WIS Per Modifier 0.05; MentalResistancePct 0 + WIL Per Modifier 0.10."))
struct ARCRPGMECHANICS_API FArcRPGAttributeHandler_Linear : public FArcAttributeHandler
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Derivation", meta = (ToolTip = "Constant part of the base value."))
	float Base = 0.f;

	UPROPERTY(EditAnywhere, Category = "Derivation")
	TArray<FArcRPGChanceTerm> Terms;

	virtual void Initialize(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float NewValue) const override;

	virtual void OnObservedAttributeChanged(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, const FArcAttributeRef& ChangedRef,
		float OldValue, float NewValue) const override;

	virtual void GatherInputs(TArray<FArcAttributeRef>& OutInputs) const override;

private:
	float ComputeValue(FMassEntityManager& EntityManager, FMassEntityHandle Entity) const;
};

USTRUCT(meta = (Hidden, ToolTip = "Base of the resource maximum handlers. Use a concrete subclass."))
struct ARCRPGMECHANICS_API FArcRPGAttributeHandler_DerivedValue : public FArcAttributeHandler
{
	GENERATED_BODY()

	virtual void Initialize(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float NewValue) const override;

	virtual void OnObservedAttributeChanged(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, const FArcAttributeRef& ChangedRef,
		float OldValue, float NewValue) const override;

protected:
	virtual bool ComputeValue(FMassEntityManager& EntityManager, FMassEntityHandle Entity, float& OutValue) const;

	virtual void OnInitialized(FArcAttributeHandlerContext& Context, float Value) const;
};

USTRUCT(meta = (Hidden, ToolTip = "Base of the resource maximum handlers. Use a concrete subclass."))
struct ARCRPGMECHANICS_API FArcRPGAttributeHandler_ResourceMax : public FArcRPGAttributeHandler_DerivedValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Resource", meta = (ToolTip = "At spawn, sets the resource to the maximum. Off: the resource is only lowered to the maximum."))
	bool bFillResourceOnSpawn = true;

	virtual void PostChange(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) const override;

protected:
	virtual void OnInitialized(FArcAttributeHandlerContext& Context, float Value) const override;

	virtual FArcAttributeRef GetResourceAttribute() const;
};

USTRUCT(meta = (DisplayName = "RPG Max Health", ToolTip = "Assign to FArcAttributeSet_Health::MaxHealth. Writes the base value Base + Per Modifier * modifier(Constitution) (attributes.yaml hp, D-73). Fills Health at spawn and lowers Health when MaxHealth drops below it."))
struct ARCRPGMECHANICS_API FArcRPGAttributeHandler_MaxHealth : public FArcRPGAttributeHandler_ResourceMax
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Derivation", meta = (ToolTip = "attributes.yaml Constitution hp base."))
	float Base = 100.f;

	UPROPERTY(EditAnywhere, Category = "Derivation", meta = (ToolTip = "attributes.yaml Constitution hp per_mod."))
	float PerModifier = 10.f;

	virtual void GatherInputs(TArray<FArcAttributeRef>& OutInputs) const override;

protected:
	virtual bool ComputeValue(FMassEntityManager& EntityManager, FMassEntityHandle Entity, float& OutValue) const override;

	virtual FArcAttributeRef GetResourceAttribute() const override;
};

USTRUCT(meta = (Hidden, ToolTip = "Base of the Fatigue pooled maximum handlers. Use a concrete subclass."))
struct ARCRPGMECHANICS_API FArcRPGAttributeHandler_FatiguePooledMax : public FArcRPGAttributeHandler_ResourceMax
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Derivation", meta = (ToolTip = "attributes.yaml pool base."))
	float Base = 0.f;

	UPROPERTY(EditAnywhere, Category = "Derivation", meta = (ToolTip = "attributes.yaml pool per_point of the governing attribute."))
	float PerPoint = 0.f;

	UPROPERTY(EditAnywhere, Category = "Fatigue", meta = (ClampMin = "0", ToolTip = "survival.yaml fatigue pool_max_penalty_per_point."))
	float FatiguePenaltyPerPoint = 0.005f;

	UPROPERTY(EditAnywhere, Category = "Fatigue", meta = (ClampMin = "0", ClampMax = "1", ToolTip = "Lowest pool multiplier (survival.yaml pool_stacking floor)."))
	float PoolFloor = 0.25f;

	virtual void GatherInputs(TArray<FArcAttributeRef>& OutInputs) const override;

protected:
	virtual bool ComputeValue(FMassEntityManager& EntityManager, FMassEntityHandle Entity, float& OutValue) const override;

	virtual FArcAttributeRef GetInputAttribute() const;
};

USTRUCT(meta = (DisplayName = "RPG Max Stamina", ToolTip = "Assign to FArcAttributeSet_RPGStamina::MaxStamina. Writes the base value (Base + Per Point * Constitution) * max(Pool Floor, 1 - Fatigue * Fatigue Penalty Per Point); Fatigue is FArcSurvivalAttributeFragment::Fatigue, 0 when missing. Fills Stamina at spawn and lowers Stamina when MaxStamina drops below it."))
struct ARCRPGMECHANICS_API FArcRPGAttributeHandler_MaxStamina : public FArcRPGAttributeHandler_FatiguePooledMax
{
	GENERATED_BODY()

	FArcRPGAttributeHandler_MaxStamina();

protected:
	virtual FArcAttributeRef GetResourceAttribute() const override;

	virtual FArcAttributeRef GetInputAttribute() const override;
};

USTRUCT(meta = (DisplayName = "RPG Max Energy", ToolTip = "Assign to FArcAttributeSet_RPGEnergy::MaxEnergy. Writes the base value (Base + Per Point * Magic) * max(Pool Floor, 1 - Fatigue * Fatigue Penalty Per Point); Fatigue is FArcSurvivalAttributeFragment::Fatigue, 0 when missing. Fills Energy at spawn and lowers Energy when MaxEnergy drops below it."))
struct ARCRPGMECHANICS_API FArcRPGAttributeHandler_MaxEnergy : public FArcRPGAttributeHandler_FatiguePooledMax
{
	GENERATED_BODY()

	FArcRPGAttributeHandler_MaxEnergy();

protected:
	virtual FArcAttributeRef GetResourceAttribute() const override;

	virtual FArcAttributeRef GetInputAttribute() const override;
};

USTRUCT(meta = (DisplayName = "RPG Max Entropy", ToolTip = "Assign to FArcAttributeSet_RPGEntropy::MaxEntropy. Writes the base value Threshold Base + Threshold Per Point * Willpower + Headroom (entropy.yaml threshold and maximum, D-94). Lowers Entropy when MaxEntropy drops below it."))
struct ARCRPGMECHANICS_API FArcRPGAttributeHandler_MaxEntropy : public FArcRPGAttributeHandler_ResourceMax
{
	GENERATED_BODY()

	FArcRPGAttributeHandler_MaxEntropy();

	UPROPERTY(EditAnywhere, Category = "Derivation", meta = (ToolTip = "entropy.yaml threshold base; keep equal to the RPG Linear handler on ThresholdEntropy."))
	float ThresholdBase = 50.f;

	UPROPERTY(EditAnywhere, Category = "Derivation", meta = (ToolTip = "entropy.yaml threshold per_point of Willpower; keep equal to the RPG Linear handler on ThresholdEntropy."))
	float ThresholdPerPoint = 5.f;

	UPROPERTY(EditAnywhere, Category = "Derivation", meta = (ClampMin = "0", ToolTip = "entropy.yaml maximum above_threshold (D-94)."))
	float Headroom = 80.f;

	virtual void GatherInputs(TArray<FArcAttributeRef>& OutInputs) const override;

protected:
	virtual bool ComputeValue(FMassEntityManager& EntityManager, FMassEntityHandle Entity, float& OutValue) const override;

	virtual FArcAttributeRef GetResourceAttribute() const override;
};
