// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcBouncingProjectileTrait.h"

#include "MassEntityTemplateRegistry.h"
#include "Mass/EntityUtils.h"
#include "MassWorldUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcBouncingProjectileTrait)

void UArcBouncingProjectileTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	Super::BuildTemplate(BuildContext, World);
	BuildContext.AddTag<FArcBouncingProjectileTag>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct BounceConfigFragment = EntityManager.GetOrCreateConstSharedFragment(BounceConfig);
	BuildContext.AddConstSharedFragment(BounceConfigFragment);
}
