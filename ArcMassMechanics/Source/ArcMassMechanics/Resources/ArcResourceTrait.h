// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcResourceTrait.generated.h"

class UArcResourceConfig;

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Resources"))
class ARCMASSMECHANICS_API UArcResourceTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Resources", meta = (ToolTip = "Required. Without a config the trait adds nothing. The entity also needs the attribute sets holding each resource's Value, metas and rate inputs, a handler config wiring each meta, and owned tags for RateMultipliers and BlockedBy."))
	TObjectPtr<UArcResourceConfig> Config = nullptr;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
	virtual bool ValidateTemplate(const FMassEntityTemplateBuildContext& BuildContext, const UWorld& World, FAdditionalTraitRequirements& OutTraitRequirements) const override;
};
