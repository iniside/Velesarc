// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcKnowledgeStaticTrait.h"

#include "Mass/EntityFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"

void UArcKnowledgeStaticTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddTag<FArcKnowledgeStaticTag>();
	BuildContext.RequireFragment<FTransformFragment>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(StaticConfig);
	BuildContext.AddConstSharedFragment(ConfigFragment);
}
