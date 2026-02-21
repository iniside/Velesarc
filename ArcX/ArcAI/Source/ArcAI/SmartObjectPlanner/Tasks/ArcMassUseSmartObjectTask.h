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

	UPROPERTY(EditAnywhere, Category = Input)
	FArcMassEntityHandleWrapper SmartObjectEntityHandle;

	UPROPERTY(EditAnywhere, Category = Input)
	FSmartObjectHandle SmartObjectHandle;

	UPROPERTY(EditAnywhere, Category = Input)
	FSmartObjectClaimHandle SmartObjectClaimHandle;

	UPROPERTY()
	TObjectPtr<UGameplayBehavior> GameplayBehavior;

	UPROPERTY()
	FArcGameplayInteractionContext GameplayInteractionContext;
};

/**
* Task that uses a claimed Smart Object slot, running GameplayBehavior and/or MassBehavior definitions.
*/
USTRUCT(meta = (DisplayName = "Arc Mass Use Smart Object", Category = "Common"))
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
};
