// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcProjectileTraitBase.h"

#include "MassCommonFragments.h"
#include "MassActorSubsystem.h"
#include "MassEntityTemplateRegistry.h"
#include "Mass/EntityUtils.h"
#include "MassWorldUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcProjectileTraitBase)

void UArcProjectileTraitBase::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.RequireFragment<FMassActorFragment>();

	BuildContext.AddFragment<FArcProjectileFragment>();
	BuildContext.AddFragment<FArcProjectileCollisionFilterFragment>();
	BuildContext.AddTag<FArcProjectileTag>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(ProjectileConfig);
	BuildContext.AddConstSharedFragment(ConfigFragment);
}
