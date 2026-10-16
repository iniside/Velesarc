// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTargetedProjectileTrait.h"

#include "MassEntityTemplateRegistry.h"
#include "Mass/EntityUtils.h"
#include "MassWorldUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTargetedProjectileTrait)

void UArcTargetedProjectileTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	Super::BuildTemplate(BuildContext, World);
	BuildContext.AddFragment<FArcTargetedProjectileFragment>();
	BuildContext.AddTag<FArcTargetedProjectileTag>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct TargetedConfigFragment = EntityManager.GetOrCreateConstSharedFragment(TargetedConfig);
	BuildContext.AddConstSharedFragment(TargetedConfigFragment);
}
