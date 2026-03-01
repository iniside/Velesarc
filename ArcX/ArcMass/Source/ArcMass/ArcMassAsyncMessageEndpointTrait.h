// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcMassAsyncMessageEndpointTrait.generated.h"

/**
 * Trait that adds an async message endpoint to a Mass entity.
 * The endpoint is created by the add observer and destroyed by the remove observer.
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Async Message Endpoint"))
class ARCMASS_API UArcMassAsyncMessageEndpointTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
