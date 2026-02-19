// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcMassEntityVisualization.h"
#include "ArcMassEntityVisualizationTrait.generated.h"

UCLASS(BlueprintType, EditInlineNew, CollapseCategories)
class ARCMASS_API UArcEntityVisualizationTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	FArcVisConfigFragment VisualizationConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
