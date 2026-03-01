// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcMobileVisualization.h"
#include "ArcMobileVisualizationTrait.generated.h"

// ---------------------------------------------------------------------------
// Entity Trait — opts a moving entity into the mobile visualization system.
// ---------------------------------------------------------------------------

UCLASS(BlueprintType, EditInlineNew, CollapseCategories)
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

// ---------------------------------------------------------------------------
// Source Trait — marks an entity as an LOD source (player, camera).
// ---------------------------------------------------------------------------

UCLASS(BlueprintType, EditInlineNew, CollapseCategories)
class ARCMASS_API UArcMobileVisSourceTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
