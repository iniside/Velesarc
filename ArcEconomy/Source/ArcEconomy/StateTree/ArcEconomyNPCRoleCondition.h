// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "ArcEconomyTypes.h"
#include "ArcEconomyNPCRoleCondition.generated.h"

USTRUCT()
struct FArcEconomyNPCRoleConditionInstanceData
{
	GENERATED_BODY()
};

/**
 * StateTree condition: checks whether the NPC entity's current economy role
 * (from FArcEconomyNPCFragment::Role) matches the configured RequiredRole.
 */
USTRUCT(meta = (DisplayName = "Arc Economy NPC Role", Category = "Arc|Economy"))
struct ARCECONOMY_API FArcEconomyNPCRoleCondition : public FMassStateTreeConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcEconomyNPCRoleConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	/** The role to test against. */
	UPROPERTY(EditAnywhere, Category = "Condition")
	EArcEconomyNPCRole RequiredRole = EArcEconomyNPCRole::Idle;

	/** If true, the condition returns true when the role does NOT match. */
	UPROPERTY(EditAnywhere, Category = "Condition")
	bool bInvert = false;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};
