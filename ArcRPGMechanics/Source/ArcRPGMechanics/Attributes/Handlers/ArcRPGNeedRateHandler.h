// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "ArcRPGNeedRateHandler.generated.h"

USTRUCT(meta = (DisplayName = "RPG Need Rates", ToolTip = "Assign to FArcAttributeSet_RPGCoreStats::Constitution. On spawn and whenever Constitution, Strength or Willpower changes, writes ChangeRate of FArcHungerNeedFragment, FArcThirstNeedFragment and FArcFatigueNeedFragment as rate per game hour * (1 + sum of coefficient * modifier), converted to per real second (survival.yaml meters), and Resistance of the hunger and thirst fragments as Threshold Shift Per Modifier * modifier(Willpower) (survival.yaml threshold_shift). Missing fragments are skipped."))
struct ARCRPGMECHANICS_API FArcRPGAttributeHandler_NeedRates : public FArcAttributeHandler
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Hunger", meta = (ClampMin = "0", ToolTip = "survival.yaml hunger rate base_per_h."))
	float HungerPerGameHour = 4.f;

	UPROPERTY(EditAnywhere, Category = "Hunger", meta = (ToolTip = "survival.yaml hunger attribute_coefficients CON."))
	float HungerConstitutionPerModifier = -0.06f;

	UPROPERTY(EditAnywhere, Category = "Hunger", meta = (ToolTip = "survival.yaml hunger attribute_coefficients STR."))
	float HungerStrengthPerModifier = 0.03f;

	UPROPERTY(EditAnywhere, Category = "Thirst", meta = (ClampMin = "0", ToolTip = "survival.yaml thirst rate base_per_h."))
	float ThirstPerGameHour = 6.f;

	UPROPERTY(EditAnywhere, Category = "Thirst", meta = (ToolTip = "survival.yaml thirst attribute_coefficients CON."))
	float ThirstConstitutionPerModifier = -0.06f;

	UPROPERTY(EditAnywhere, Category = "Fatigue", meta = (ClampMin = "0", ToolTip = "survival.yaml fatigue sources wake base_per_h."))
	float FatiguePerGameHour = 3.f;

	UPROPERTY(EditAnywhere, Category = "Fatigue", meta = (ToolTip = "survival.yaml fatigue sources wake attribute_coefficients CON."))
	float FatigueConstitutionPerModifier = -0.05f;

	UPROPERTY(EditAnywhere, Category = "Resistance", meta = (ToolTip = "survival.yaml attribute_modifiers threshold_shift per_mod of Willpower. Hunger and thirst only."))
	float ThresholdShiftPerModifier = 3.f;

	virtual void Initialize(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float NewValue) const override;

	virtual void PostChange(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) const override;

	virtual void OnObservedAttributeChanged(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, const FArcAttributeRef& ChangedRef,
		float OldValue, float NewValue) const override;

	virtual void GatherInputs(TArray<FArcAttributeRef>& OutInputs) const override;

private:
	void WriteNeedRates(const FArcAttributeHandlerContext& Context, float Constitution) const;
};
