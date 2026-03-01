// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcKnowledgeTrait.h"

#include "MassEntityFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"

void UArcKnowledgeMemberTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FArcKnowledgeMemberFragment>();
	BuildContext.AddTag<FArcKnowledgeMemberTag>();
	BuildContext.RequireFragment<FTransformFragment>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(MemberConfig);
	BuildContext.AddConstSharedFragment(ConfigFragment);
}
