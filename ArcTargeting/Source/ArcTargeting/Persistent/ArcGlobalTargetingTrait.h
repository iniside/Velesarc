#pragma once

#include "MassEntityTraitBase.h"
#include "Persistent/ArcGlobalTargetingFragments.h"
#include "ArcGlobalTargetingTrait.generated.h"

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Global Targeting", Category = "Targeting"))
class ARCTARGETING_API UArcGlobalTargetingTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	TArray<FArcGlobalTargetingConfigEntry> Entries;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
