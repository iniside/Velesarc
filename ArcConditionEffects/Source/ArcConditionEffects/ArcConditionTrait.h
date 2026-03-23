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

// Group A: Hysteresis — conditions that ramp up and decay with thermal-style curves.

/** Trait that adds the burning condition to a Mass entity.
 *  Thermal hysteresis condition — entity accumulates heat and ignites above threshold. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Burning Condition", Category = "Conditions"))
class ARCCONDITIONEFFECTS_API UArcBurningConditionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FArcBurningConditionConfig ConditionConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

/** Trait that adds the bleeding condition to a Mass entity.
 *  Biological hysteresis condition — entity loses health over time from open wounds. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Bleeding Condition", Category = "Conditions"))
class ARCCONDITIONEFFECTS_API UArcBleedingConditionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FArcBleedingConditionConfig ConditionConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

/** Trait that adds the chilled condition to a Mass entity.
 *  Thermal hysteresis condition — entity accumulates cold, slowing movement and actions. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Chilled Condition", Category = "Conditions"))
class ARCCONDITIONEFFECTS_API UArcChilledConditionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FArcChilledConditionConfig ConditionConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

/** Trait that adds the shocked condition to a Mass entity.
 *  Environmental hysteresis condition — electrical buildup causes periodic stuns. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Shocked Condition"), Category = "Conditions")
class ARCCONDITIONEFFECTS_API UArcShockedConditionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FArcShockedConditionConfig ConditionConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

/** Trait that adds the poisoned condition to a Mass entity.
 *  Biological hysteresis condition — toxins deal sustained damage and reduce healing. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Poisoned Condition"))
class ARCCONDITIONEFFECTS_API UArcPoisonedConditionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FArcPoisonedConditionConfig ConditionConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

/** Trait that adds the diseased condition to a Mass entity.
 *  Biological hysteresis condition — infection weakens stats and may spread to nearby entities. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Diseased Condition", Category = "Conditions"))
class ARCCONDITIONEFFECTS_API UArcDiseasedConditionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FArcDiseasedConditionConfig ConditionConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

/** Trait that adds the weakened condition to a Mass entity.
 *  Biological hysteresis condition — reduces entity's damage output and resistances. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Weakened Condition", Category = "Conditions"))
class ARCCONDITIONEFFECTS_API UArcWeakenedConditionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FArcWeakenedConditionConfig ConditionConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

// Group B: Linear — conditions that decay at a constant rate over time.

/** Trait that adds the oiled condition to a Mass entity.
 *  Linear decay condition — coats the entity in oil, increasing fire vulnerability. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Oiled Condition", Category = "Conditions"))
class ARCCONDITIONEFFECTS_API UArcOiledConditionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FArcOiledConditionConfig ConditionConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

/** Trait that adds the wet condition to a Mass entity.
 *  Linear decay condition — soaks the entity, increasing shock vulnerability and reducing fire. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Wet Condition", Category = "Conditions"))
class ARCCONDITIONEFFECTS_API UArcWetConditionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FArcWetConditionConfig ConditionConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

/** Trait that adds the corroded condition to a Mass entity.
 *  Linear decay condition — degrades armor and equipment durability over time. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Corroded Condition", Category = "Conditions"))
class ARCCONDITIONEFFECTS_API UArcCorrodedConditionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FArcCorrodedConditionConfig ConditionConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

// Group C: Environmental — conditions triggered by the surrounding environment.

/** Trait that adds the blinded condition to a Mass entity.
 *  Environmental condition — reduces perception range and accuracy. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Blinded Condition", Category = "Conditions"))
class ARCCONDITIONEFFECTS_API UArcBlindedConditionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FArcBlindedConditionConfig ConditionConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

/** Trait that adds the suffocating condition to a Mass entity.
 *  Environmental condition — entity takes damage from lack of breathable air. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Suffocating Condition", Category = "Conditions"))
class ARCCONDITIONEFFECTS_API UArcSuffocatingConditionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FArcSuffocatingConditionConfig ConditionConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

/** Trait that adds the exhausted condition to a Mass entity.
 *  Environmental condition — drains stamina and slows movement from overexertion. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Exhausted Condition", Category = "Conditions"))
class ARCCONDITIONEFFECTS_API UArcExhaustedConditionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FArcExhaustedConditionConfig ConditionConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

#undef ARC_DECLARE_CONDITION_TRAIT

// ---------------------------------------------------------------------------
// Composite Trait — adds all 13 conditions at once (convenience)
// ---------------------------------------------------------------------------

/** Trait that adds all 13 condition types to a Mass entity at once.
 *  Convenience composite — configures thermal, biological, linear, and environmental conditions. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc All Conditions", Category = "Conditions"))
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
