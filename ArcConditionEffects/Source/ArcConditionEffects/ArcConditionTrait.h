// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcConditionFragments.h"
#include "MassEntityTraitBase.h"

#include "ArcConditionTrait.generated.h"

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Conditions", Category = "Conditions"))
class ARCCONDITIONEFFECTS_API UArcConditionsTrait : public UMassEntityTraitBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions|Thermal")
    FArcConditionConfig BurningConfig = ArcConditionDefaultConfigs::Make(EArcConditionGroup::GroupA_Hysteresis, 20.f, 3.f, 6.f, 2.f, 5.f, 0.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions|Thermal")
    FArcConditionConfig ChilledConfig = ArcConditionDefaultConfigs::Make(EArcConditionGroup::GroupA_Hysteresis, 30.f, 2.f, 5.f, 2.f, 5.f, 0.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions|Thermal")
    FArcConditionConfig OiledConfig = ArcConditionDefaultConfigs::Make(EArcConditionGroup::GroupB_Linear, 0.f, 1.f, 0.f, 0.f, 1.f, 0.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions|Thermal")
    FArcConditionConfig WetConfig = ArcConditionDefaultConfigs::Make(EArcConditionGroup::GroupB_Linear, 0.f, 2.f, 0.f, 0.f, 1.f, 0.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions|Biological")
    FArcConditionConfig BleedingConfig = ArcConditionDefaultConfigs::Make(EArcConditionGroup::GroupA_Hysteresis, 25.f, 2.f, 8.f, 2.f, 5.f, 0.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions|Biological")
    FArcConditionConfig PoisonedConfig = ArcConditionDefaultConfigs::Make(EArcConditionGroup::GroupA_Hysteresis, 20.f, 1.f, 5.f, 2.f, 5.f, 0.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions|Biological")
    FArcConditionConfig DiseasedConfig = ArcConditionDefaultConfigs::Make(EArcConditionGroup::GroupA_Hysteresis, 25.f, 0.5f, 10.f, 2.f, 5.f, 0.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions|Biological")
    FArcConditionConfig WeakenedConfig = ArcConditionDefaultConfigs::Make(EArcConditionGroup::GroupA_Hysteresis, 20.f, 2.f, 10.f, 2.f, 5.f, 0.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions|Environmental")
    FArcConditionConfig ShockedConfig = ArcConditionDefaultConfigs::Make(EArcConditionGroup::GroupA_Hysteresis, 30.f, 4.f, 5.f, 2.f, 5.f, 0.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions|Environmental")
    FArcConditionConfig BlindedConfig = ArcConditionDefaultConfigs::Make(EArcConditionGroup::GroupC_Environmental, 0.f, 5.f, 4.f, 1.f, 5.f, 0.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions|Environmental")
    FArcConditionConfig SuffocatingConfig = ArcConditionDefaultConfigs::Make(EArcConditionGroup::GroupC_Environmental, 0.f, 10.f, 3.f, 1.f, 5.f, 0.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions|Environmental")
    FArcConditionConfig ExhaustedConfig = ArcConditionDefaultConfigs::Make(EArcConditionGroup::GroupC_Environmental, 0.f, 3.f, 0.f, 0.f, 1.f, 0.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions|Environmental")
    FArcConditionConfig CorrodedConfig = ArcConditionDefaultConfigs::Make(EArcConditionGroup::GroupB_Linear, 0.f, 0.5f, 0.f, 0.f, 1.f, 0.f);

    virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
