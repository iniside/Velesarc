// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "StateTreeDelegate.h"
#include "ArcListenNPCRoleChangedTask.generated.h"

USTRUCT()
struct FArcListenNPCRoleChangedTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnRoleChanged;

	FDelegateHandle RoleChangedHandle;
};

USTRUCT(meta = (DisplayName = "Listen NPC Role Changed", Category = "Arc|Economy|NPC"))
struct ARCECONOMY_API FArcListenNPCRoleChangedTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcListenNPCRoleChangedTaskInstanceData;

	FArcListenNPCRoleChangedTask();

protected:
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Task"); }
	virtual FColor GetIconColor() const override { return FColor(220, 160, 60); }
#endif
};
