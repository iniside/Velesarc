// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcMass/Config/ArcMassEntityTraitBase.h"
#include "Decision/ArcTacticsCompiledData.h"

#include "ArcTacticsTrait.generated.h"

class UArcTacticsConfig;

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Tactics", Category = "Tactics"))
class ARCTACTICS_API UArcTacticsTrait : public UArcMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ToolTip = "Decision packages active from spawn. Each config contributes its own global tasks and candidates to one shared decision."))
	TArray<TObjectPtr<UArcTacticsConfig>> Configs;

	UPROPERTY(EditAnywhere, Category = "Tactics", meta = (ToolTip = "Decision timing, selection and history settings shared by every decision of this entity."))
	FArcTacticsDecisionSettings Settings;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
	virtual bool ValidateTemplate(const FMassEntityTemplateBuildContext& BuildContext, const UWorld& World, FAdditionalTraitRequirements& OutTraitRequirements) const override;
};
