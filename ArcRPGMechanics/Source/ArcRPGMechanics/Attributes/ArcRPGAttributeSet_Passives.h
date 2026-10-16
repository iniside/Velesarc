// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "ArcRPGAttributeSet_Passives.generated.h"

USTRUCT(BlueprintType)
struct ARCRPGMECHANICS_API FArcRPGAttributeSet_Passives : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Passives", meta = (ToolTip = "Carry capacity in kilograms from Strength (attributes.yaml carry_kg). Written by its RPG Stat Scaled Value handler."))
	FArcAttribute CarryKg;

	UPROPERTY(EditAnywhere, Category = "Passives", meta = (ToolTip = "Attack speed bonus as a fraction (0.12 = +12%) from Dexterity (attributes.yaml attack_speed_pct). Written by its RPG Stat Scaled Value handler."))
	FArcAttribute AttackSpeedPct;

	UPROPERTY(EditAnywhere, Category = "Passives", meta = (ToolTip = "Movement speed bonus as a fraction from Agility (attributes.yaml movement_speed_pct). Written by its RPG Stat Scaled Value handler."))
	FArcAttribute MovementSpeedPct;

	UPROPERTY(EditAnywhere, Category = "Passives", meta = (ToolTip = "Detection radius in meters from Perception (attributes.yaml detection_radius_m). Written by its RPG Stat Scaled Value handler."))
	FArcAttribute DetectionRadius;

	UPROPERTY(EditAnywhere, Category = "Passives", meta = (ToolTip = "Non-magical healing bonus as a fraction from Wisdom (attributes.yaml healing_bonus_pct). Written by its RPG Stat Scaled Value handler."))
	FArcAttribute HealingBonusPct;

	UPROPERTY(EditAnywhere, Category = "Passives", meta = (ToolTip = "Food and drink recovery multiplier from Wisdom (survival.yaml consumption_efficiency). Written by its RPG Stat Scaled Value handler."))
	FArcAttribute FoodDrinkEfficiency;

	UPROPERTY(EditAnywhere, Category = "Passives", meta = (ToolTip = "Companion effectiveness bonus as a fraction from Wisdom (attributes.yaml companion_effectiveness_pct). Written by its RPG Stat Scaled Value handler."))
	FArcAttribute CompanionEffectivenessPct;

	UPROPERTY(EditAnywhere, Category = "Passives", meta = (ToolTip = "Mental resistance as a fraction from Willpower (attributes.yaml mental_resistance_pct). Written by its RPG Stat Scaled Value handler."))
	FArcAttribute MentalResistancePct;

	UPROPERTY(EditAnywhere, Category = "Passives", meta = (ToolTip = "Healing received modifier as a fraction (0.1 = +10%). Fraction; no consumer yet."))
	FArcAttribute HealingReceivedPct;

	UPROPERTY(EditAnywhere, Category = "Passives", meta = (ToolTip = "Cast speed bonus as a fraction. Fraction; no consumer yet."))
	FArcAttribute CastSpeedPct;

	UPROPERTY(EditAnywhere, Category = "Passives", meta = (ToolTip = "Aftercast duration modifier as a fraction. Fraction; no consumer yet."))
	FArcAttribute AftercastPct;

	UPROPERTY(EditAnywhere, Category = "Passives", meta = (ToolTip = "Elemental reaction strength bonus as a fraction. Fraction; no consumer yet."))
	FArcAttribute ReactionStrengthPct;

	UPROPERTY(EditAnywhere, Category = "Passives", meta = (ToolTip = "Stamina cost modifier as a fraction. Fraction; no consumer yet."))
	FArcAttribute StaminaCostPct;

	UPROPERTY(EditAnywhere, Category = "Passives", meta = (ToolTip = "Cascade reset rate bonus as a fraction. Fraction; no consumer yet."))
	FArcAttribute CascadeResetRatePct;

	UPROPERTY(EditAnywhere, Category = "Passives", meta = (ToolTip = "Hunger rate modifier as a fraction. Fraction; no consumer yet."))
	FArcAttribute HungerRatePct;

	UPROPERTY(EditAnywhere, Category = "Passives", meta = (ToolTip = "Thirst rate modifier as a fraction. Fraction; no consumer yet."))
	FArcAttribute ThirstRatePct;

	UPROPERTY(EditAnywhere, Category = "Passives", meta = (ToolTip = "Fatigue rate modifier as a fraction. Fraction; no consumer yet."))
	FArcAttribute FatigueRatePct;

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcRPGAttributeSet_Passives, CarryKg)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcRPGAttributeSet_Passives, AttackSpeedPct)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcRPGAttributeSet_Passives, MovementSpeedPct)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcRPGAttributeSet_Passives, DetectionRadius)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcRPGAttributeSet_Passives, HealingBonusPct)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcRPGAttributeSet_Passives, FoodDrinkEfficiency)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcRPGAttributeSet_Passives, CompanionEffectivenessPct)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcRPGAttributeSet_Passives, MentalResistancePct)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcRPGAttributeSet_Passives, HealingReceivedPct)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcRPGAttributeSet_Passives, CastSpeedPct)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcRPGAttributeSet_Passives, AftercastPct)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcRPGAttributeSet_Passives, ReactionStrengthPct)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcRPGAttributeSet_Passives, StaminaCostPct)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcRPGAttributeSet_Passives, CascadeResetRatePct)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcRPGAttributeSet_Passives, HungerRatePct)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcRPGAttributeSet_Passives, ThirstRatePct)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcRPGAttributeSet_Passives, FatigueRatePct)
};

template <>
struct TMassFragmentTraits<FArcRPGAttributeSet_Passives>
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
