// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEconomyNPCRoleCondition.h"

#include "MassStateTreeExecutionContext.h"
#include "Mass/ArcEconomyFragments.h"

bool FArcEconomyNPCRoleCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);

	FMassEntityManager& EntityManager = MassCtx.GetEntityManager();
	const FMassEntityHandle Entity = MassCtx.GetEntity();

	bool bMatch = false;
	if (const FArcEconomyNPCFragment* Fragment = EntityManager.GetFragmentDataPtr<FArcEconomyNPCFragment>(Entity))
	{
		bMatch = (Fragment->Role == RequiredRole);
	}

	return bInvert ? !bMatch : bMatch;
}

#if WITH_EDITOR
FText FArcEconomyNPCRoleCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const UEnum* RoleEnum = StaticEnum<EArcEconomyNPCRole>();
	const FString RoleName = RoleEnum ? RoleEnum->GetDisplayNameTextByValue(static_cast<int64>(RequiredRole)).ToString() : TEXT("Unknown");

	if (bInvert)
	{
		return FText::Format(NSLOCTEXT("ArcEconomy", "NPCRoleNotDesc", "Role != {0}"), FText::FromString(RoleName));
	}
	return FText::Format(NSLOCTEXT("ArcEconomy", "NPCRoleDesc", "Role == {0}"), FText::FromString(RoleName));
}
#endif
