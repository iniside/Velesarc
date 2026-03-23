// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcVisLifecycle.h"
#include "ArcVisLifecycleTrait.generated.h"

/** Trait that adds visualization lifecycle configuration to a Mass entity.
 *  Controls when and how the entity's visual representation is created and destroyed. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Vis Lifecycle", Category = "Visualization"))
class ARCMASS_API UArcVisLifecycleTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle")
	FArcVisLifecycleConfigFragment LifecycleVisConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
