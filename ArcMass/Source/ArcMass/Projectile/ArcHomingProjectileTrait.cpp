// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcHomingProjectileTrait.h"

#include "MassEntityTemplateRegistry.h"
#include "Mass/EntityUtils.h"
#include "MassWorldUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcHomingProjectileTrait)

void UArcHomingProjectileTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	Super::BuildTemplate(BuildContext, World);
	BuildContext.AddFragment<FArcProjectileHomingFragment>();
	BuildContext.AddTag<FArcHomingProjectileTag>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct HomingConfigFragment = EntityManager.GetOrCreateConstSharedFragment(HomingConfig);
	BuildContext.AddConstSharedFragment(HomingConfigFragment);
}
