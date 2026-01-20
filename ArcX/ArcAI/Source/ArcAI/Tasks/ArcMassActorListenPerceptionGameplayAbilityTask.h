// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassStateTreeRunEnvQueryTask.h"
#include "AsyncMessageHandle.h"
#include "MassActorSubsystem.h"
#include "StateTreeDelegate.h"
#include "Engine/TimerHandle.h"
#include "Perception/ArcAISense_GameplayAbility.h"

#include "ArcMassActorListenPerceptionGameplayAbilityTask.generated.h"

USTRUCT()
struct FArcMassActorListenPerceptionGameplayAbilityTaskInstanceData
{
	GENERATED_BODY()

	/** Delay before the task ends. Default (0 or any negative) will run indefinitely, so it requires a transition in the state tree to stop it. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float Duration = 0.f;

	UPROPERTY(EditAnywhere, Category = Output)
	FArcAIGameplayAbilityEvent Output;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	TInstancedStruct<FArcAIBaseEvent> OutputInstanced;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnResultChanged;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FAsyncMessageId MessageToListenFor = FAsyncMessageId::Invalid;
	
	FDelegateHandle PerceptionChangedDelegate;
	FTimerHandle TimerHandle;
	FAsyncMessageHandle BoundListenerHandle = FAsyncMessageHandle::Invalid;
};

USTRUCT(meta = (DisplayName = "Arc Mass Actor Listen Perception Gameplay Ability Task "))
struct FArcMassActorListenPerceptionGameplayAbilityTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()
	
	using FInstanceDataType = FArcMassActorListenPerceptionGameplayAbilityTaskInstanceData;

public:
	FArcMassActorListenPerceptionGameplayAbilityTask();
	
protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	
	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	TStateTreeExternalDataHandle<FMassActorFragment> MassActorFragment;
};
