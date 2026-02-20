// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcVisLifecycleTrait.h"

#include "MassCommonFragments.h"
#include "MassEntityTemplateRegistry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcVisLifecycleTrait)

void UArcVisLifecycleTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FArcVisLifecycleFragment>();
	BuildContext.AddTag<FArcVisLifecycleTag>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(LifecycleVisConfig);
	BuildContext.AddConstSharedFragment(ConfigFragment);
}
