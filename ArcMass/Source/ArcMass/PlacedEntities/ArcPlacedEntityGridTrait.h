// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcPlacedEntityGridTrait.generated.h"

/**
 * Optional trait that specifies which World Partition runtime grid
 * placed entities using this config should be assigned to.
 * If absent or GridName is NAME_None, placement falls back to the main grid.
 */
UCLASS()
class ARCMASS_API UArcPlacedEntityGridTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
#if WITH_EDITORONLY_DATA
	/** World Partition runtime grid name. Leave empty for the default grid. */
	UPROPERTY(EditAnywhere, Category = "PlacedEntities")
	FName GridName;
#endif

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override {}
};
