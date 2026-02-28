// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSettlementTrait.h"

#include "MassEntityFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"

void UArcSettlementMemberTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FArcSettlementMemberFragment>();
	BuildContext.AddTag<FArcSettlementMemberTag>();
	BuildContext.RequireFragment<FTransformFragment>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(MemberConfig);
	BuildContext.AddConstSharedFragment(ConfigFragment);
}
