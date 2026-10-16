// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/PlacedEntities/ArcPlacedEntityRespawnTrait.h"
#include "MassEntityTemplateRegistry.h"
#include "Mass/EntityUtils.h"
#include "MassWorldUtils.h"

void UArcPlacedEntityRespawnTrait::BuildTemplate(
    FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
    if (RespawnTimeSeconds <= 0)
    {
        return;
    }

    FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

    FArcPlacedEntityRespawnConfigFragment Config;
    Config.RespawnTimeSeconds = RespawnTimeSeconds;
    const FConstSharedStruct SharedFragment = EntityManager.GetOrCreateConstSharedFragment(Config);
    BuildContext.AddConstSharedFragment(SharedFragment);
    BuildContext.AddFragment<FArcPlacedEntityOwnerFragment>();
}
