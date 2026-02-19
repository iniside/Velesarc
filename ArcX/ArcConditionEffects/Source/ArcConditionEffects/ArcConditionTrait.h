// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcConditionFragments.h"
#include "MassEntityTraitBase.h"

#include "ArcConditionTrait.generated.h"

// ---------------------------------------------------------------------------
// Per-Condition Traits
//
// Each condition gets its own trait so entities opt into individual conditions.
// A composite trait is provided for convenience.
// ---------------------------------------------------------------------------

// ===== MACRO: Declares a per-condition trait ================================

#define ARC_DECLARE_CONDITION_TRAIT(Name) \
	UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = #Name " Condition")) \
	class ARCCONDITIONEFFECTS_API UArc##Name##ConditionTrait : public UMassEntityTraitBase \
	{ \
		GENERATED_BODY() \
	public: \
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition") \
		FArc##Name##ConditionConfig ConditionConfig; \
		\
		virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override; \
	};

// Group A: Hysteresis
ARC_DECLARE_CONDITION_TRAIT(Burning)
ARC_DECLARE_CONDITION_TRAIT(Bleeding)
ARC_DECLARE_CONDITION_TRAIT(Chilled)
ARC_DECLARE_CONDITION_TRAIT(Shocked)
ARC_DECLARE_CONDITION_TRAIT(Poisoned)
ARC_DECLARE_CONDITION_TRAIT(Diseased)
ARC_DECLARE_CONDITION_TRAIT(Weakened)

// Group B: Linear
ARC_DECLARE_CONDITION_TRAIT(Oiled)
ARC_DECLARE_CONDITION_TRAIT(Wet)
ARC_DECLARE_CONDITION_TRAIT(Corroded)

// Group C: Environmental
ARC_DECLARE_CONDITION_TRAIT(Blinded)
ARC_DECLARE_CONDITION_TRAIT(Suffocating)
ARC_DECLARE_CONDITION_TRAIT(Exhausted)

#undef ARC_DECLARE_CONDITION_TRAIT

// ---------------------------------------------------------------------------
// Composite Trait — adds all 13 conditions at once (convenience)
// ---------------------------------------------------------------------------

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "All Conditions"))
class ARCCONDITIONEFFECTS_API UArcAllConditionsTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions|Thermal")
	FArcBurningConditionConfig BurningConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions|Thermal")
	FArcChilledConditionConfig ChilledConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions|Thermal")
	FArcOiledConditionConfig OiledConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions|Thermal")
	FArcWetConditionConfig WetConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions|Biological")
	FArcBleedingConditionConfig BleedingConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions|Biological")
	FArcPoisonedConditionConfig PoisonedConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions|Biological")
	FArcDiseasedConditionConfig DiseasedConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions|Biological")
	FArcWeakenedConditionConfig WeakenedConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions|Environmental")
	FArcShockedConditionConfig ShockedConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions|Environmental")
	FArcBlindedConditionConfig BlindedConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions|Environmental")
	FArcSuffocatingConditionConfig SuffocatingConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions|Environmental")
	FArcExhaustedConditionConfig ExhaustedConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions|Environmental")
	FArcCorrodedConditionConfig CorrodedConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
