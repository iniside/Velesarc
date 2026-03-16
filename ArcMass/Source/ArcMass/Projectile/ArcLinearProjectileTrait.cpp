// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcLinearProjectileTrait.h"

#include "MassCommonFragments.h"
#include "MassActorSubsystem.h"
#include "MassEntityTemplateRegistry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcLinearProjectileTrait)

void UArcLinearProjectileTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.RequireFragment<FMassActorFragment>();

	BuildContext.AddFragment<FArcProjectileFragment>();
	BuildContext.AddFragment<FArcProjectileCollisionFilterFragment>();
	BuildContext.AddTag<FArcProjectileTag>();
	BuildContext.AddTag<FArcLinearProjectileTag>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(ProjectileConfig);
	BuildContext.AddConstSharedFragment(ConfigFragment);
}
