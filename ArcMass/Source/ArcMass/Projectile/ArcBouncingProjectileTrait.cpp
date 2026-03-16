// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcBouncingProjectileTrait.h"

#include "MassCommonFragments.h"
#include "MassActorSubsystem.h"
#include "MassEntityTemplateRegistry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcBouncingProjectileTrait)

void UArcBouncingProjectileTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.RequireFragment<FMassActorFragment>();

	BuildContext.AddFragment<FArcProjectileFragment>();
	BuildContext.AddFragment<FArcProjectileCollisionFilterFragment>();
	BuildContext.AddTag<FArcProjectileTag>();
	BuildContext.AddTag<FArcBouncingProjectileTag>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(ProjectileConfig);
	BuildContext.AddConstSharedFragment(ConfigFragment);

	const FConstSharedStruct BounceConfigFragment = EntityManager.GetOrCreateConstSharedFragment(BounceConfig);
	BuildContext.AddConstSharedFragment(BounceConfigFragment);
}
