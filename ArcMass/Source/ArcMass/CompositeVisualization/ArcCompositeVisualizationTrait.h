// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcCompositeVisualization.h"
#include "ArcCompositeVisualizationTrait.generated.h"

UCLASS(BlueprintType, EditInlineNew, CollapseCategories,
	meta = (DisplayName = "Arc Composite Visualization", Category = "Visualization"))
class ARCMASS_API UArcCompositeVisualizationTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CompositeVisualization")
	FArcCompositeVisConfigFragment VisualizationConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
