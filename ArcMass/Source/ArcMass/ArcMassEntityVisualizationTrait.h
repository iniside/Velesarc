// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcMassEntityVisualization.h"
#include "ArcMassEntityVisualizationTrait.generated.h"

/** Trait that adds visualization configuration to a Mass entity.
 *  Controls how the entity is visually represented (mesh, materials, LOD settings). */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Entity Visualization", Category = "Visualization"))
class ARCMASS_API UArcEntityVisualizationTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	FArcVisConfigFragment VisualizationConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
