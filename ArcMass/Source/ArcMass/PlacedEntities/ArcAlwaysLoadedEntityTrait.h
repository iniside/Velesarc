// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcAlwaysLoadedEntityTrait.generated.h"

/**
 * Data-only trait that flags an entity config for always-loaded placement.
 * When present, the placement factory routes instances to AArcAlwaysLoadedEntityActor
 * instead of AArcPlacedEntityPartitionActor.
 */
UCLASS()
class ARCMASS_API UArcAlwaysLoadedEntityTrait : public UMassEntityTraitBase
{
    GENERATED_BODY()

protected:
    virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override {}
};
