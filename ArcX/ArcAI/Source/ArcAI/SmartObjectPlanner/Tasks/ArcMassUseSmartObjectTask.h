// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcGameplayInteractionContext.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassSmartObjectFragments.h"
#include "SmartObjectRuntime.h"
#include "SmartObjectTypes.h"
#include "ArcMass/ArcMassEntityHandleWrapper.h"
#include "Tasks/ArcMassStateTreeRunEnvQueryTask.h"

#include "ArcMassUseSmartObjectTask.generated.h"

class UGameplayBehavior;
class USmartObjectSubsystem;
class UMassSignalSubsystem;

USTRUCT()
struct FArcMassUseSmartObjectTaskInstanceData
{
	GENERATED_BODY()

	// Mass entity that owns the smart object being interacted with
	UPROPERTY(EditAnywhere, Category = Input)
	FArcMassEntityHandleWrapper SmartObjectEntityHandle;

	// Runtime handle identifying the smart object instance
	UPROPERTY(EditAnywhere, Category = Input)
	FSmartObjectHandle SmartObjectHandle;

	// Claim handle for the reserved slot (must be claimed before entering this task)
	UPROPERTY(EditAnywhere, Category = Input)
	FSmartObjectClaimHandle SmartObjectClaimHandle;

	// Active GameplayBehavior instance running on the slot (managed internally)
	UPROPERTY()
	TObjectPtr<UGameplayBehavior> GameplayBehavior;

	// Active GameplayInteraction context for the slot (managed internally)
	UPROPERTY()
	FArcGameplayInteractionContext GameplayInteractionContext;
};

/**
* Executes a claimed Smart Object interaction. Runs GameplayInteraction, GameplayBehavior,
* and/or MassBehavior definitions on the claimed slot. Handles the full lifecycle including
* animation and slot release on exit.
*/
USTRUCT(meta = (DisplayName = "Arc Mass Use Smart Object", Category = "Smart Object Planner"))
struct FArcMassUseSmartObjectTask: public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassUseSmartObjectTaskInstanceData;

	FArcMassUseSmartObjectTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transitio) const override;

	UPROPERTY(EditAnywhere)
	bool bAlwaysRunMassBehavior = true;

	TStateTreeExternalDataHandle<FMassSmartObjectUserFragment> SmartObjectUserHandle;
	TStateTreeExternalDataHandle<FMassMoveTargetFragment> MoveTargetHandle;

	TStateTreeExternalDataHandle<USmartObjectSubsystem> SmartObjectSubsystemHandle;
	TStateTreeExternalDataHandle<UMassSignalSubsystem> MassSignalSubsystemHandle;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};
