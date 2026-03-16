// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcHomingProjectileTrait.h"

#include "MassCommonFragments.h"
#include "MassActorSubsystem.h"
#include "MassEntityTemplateRegistry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcHomingProjectileTrait)

void UArcHomingProjectileTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.RequireFragment<FMassActorFragment>();

	BuildContext.AddFragment<FArcProjectileFragment>();
	BuildContext.AddFragment<FArcProjectileCollisionFilterFragment>();
	BuildContext.AddFragment<FArcProjectileHomingFragment>();
	BuildContext.AddTag<FArcProjectileTag>();
	BuildContext.AddTag<FArcHomingProjectileTag>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(ProjectileConfig);
	BuildContext.AddConstSharedFragment(ConfigFragment);

	const FConstSharedStruct HomingConfigFragment = EntityManager.GetOrCreateConstSharedFragment(HomingConfig);
	BuildContext.AddConstSharedFragment(HomingConfigFragment);
}
