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
	UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = Name##Condition)) \
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
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Burning Condition"))
class ARCCONDITIONEFFECTS_API UArcBurningConditionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FArcBurningConditionConfig ConditionConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = BleedingCondition))
class ARCCONDITIONEFFECTS_API UArcBleedingConditionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FArcBleedingConditionConfig ConditionConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = ChilledCondition))
class ARCCONDITIONEFFECTS_API UArcChilledConditionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FArcChilledConditionConfig ConditionConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = ShockedCondition))
class ARCCONDITIONEFFECTS_API UArcShockedConditionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FArcShockedConditionConfig ConditionConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = PoisonedCondition))
class ARCCONDITIONEFFECTS_API UArcPoisonedConditionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FArcPoisonedConditionConfig ConditionConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = DiseasedCondition))
class ARCCONDITIONEFFECTS_API UArcDiseasedConditionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FArcDiseasedConditionConfig ConditionConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = WeakenedCondition))
class ARCCONDITIONEFFECTS_API UArcWeakenedConditionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FArcWeakenedConditionConfig ConditionConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

// Group B: Linear
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = OiledCondition))
class ARCCONDITIONEFFECTS_API UArcOiledConditionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FArcOiledConditionConfig ConditionConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = WetCondition))
class ARCCONDITIONEFFECTS_API UArcWetConditionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FArcWetConditionConfig ConditionConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = CorrodedCondition))
class ARCCONDITIONEFFECTS_API UArcCorrodedConditionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FArcCorrodedConditionConfig ConditionConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

// Group C: Environmental
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = BlindedCondition))
class ARCCONDITIONEFFECTS_API UArcBlindedConditionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FArcBlindedConditionConfig ConditionConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = SuffocatingCondition))
class ARCCONDITIONEFFECTS_API UArcSuffocatingConditionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FArcSuffocatingConditionConfig ConditionConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = ExhaustedCondition))
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
