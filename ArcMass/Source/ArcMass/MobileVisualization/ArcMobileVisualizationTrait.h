// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcMobileVisualization.h"
#include "ArcMobileVisualizationTrait.generated.h"

/** Trait that opts a moving Mass entity into the mobile visualization system.
 *  Adds visualization and distance configs for LOD-based rendering of mobile entities. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Mobile Vis Entity", Category = "Visualization"))
class ARCMASS_API UArcMobileVisEntityTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MobileVisualization")
	FArcMobileVisConfigFragment VisualizationConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MobileVisualization")
	FArcMobileVisDistanceConfigFragment DistanceConfig;

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

/** Trait that marks a Mass entity as an LOD source (player or camera).
 *  Other mobile-vis entities use this source's location for distance-based LOD decisions. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Mobile Vis Source", Category = "Visualization"))
class ARCMASS_API UArcMobileVisSourceTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
