// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AsyncMessageHandle.h"
#include "MassActorSubsystem.h"
#include "MassStateTreeTypes.h"
#include "StateTreeDelegate.h"
#include "ArcMass/Messaging/ArcMassAsyncMessageEndpointFragment.h"
#include "Engine/TimerHandle.h"
#include "Perception/ArcAISense_GameplayAbility.h"

#include "ArcMassActorListenToAsyncMessageTask.generated.h"

USTRUCT()
struct FArcMassActorListenToAsyncMessageTaskInstanceData
{
	GENERATED_BODY()

	/** Optional duration in seconds. If > 0, the task auto-completes after this time. If 0 or negative, runs indefinitely until a transition fires. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float Duration = 0.f;

	/** Output: the received async message payload as a gameplay ability event. Populated when a matching message arrives. */
	UPROPERTY(EditAnywhere, Category = Output)
	FArcAIGameplayAbilityEvent Output;

	/** Output: the received async message payload as an instanced struct. Allows polymorphic event types derived from FArcAIBaseEvent. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TInstancedStruct<FArcAIBaseEvent> OutputInstanced;

	/** Delegate dispatcher that fires when a matching async message is received. Use this to trigger state tree transitions. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnResultChanged;

	/** The async message channel ID to listen on. Must be a valid message ID; messages are received on the entity's actor endpoint. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FAsyncMessageId MessageToListenFor = FAsyncMessageId::Invalid;

	/** Internal delegate handle for the message listener callback. */
	FDelegateHandle PerceptionChangedDelegate;
	/** Internal timer handle for the optional duration timeout. */
	FTimerHandle TimerHandle;
	/** Internal handle for the bound async message listener. Cleaned up on ExitState. */
	FAsyncMessageHandle BoundListenerHandle = FAsyncMessageHandle::Invalid;
};

/**
 * Latent task that listens for async gameplay messages on the entity's actor endpoint.
 *
 * On EnterState, retrieves the entity's actor via FMassActorFragment, binds a listener on the
 * specified MessageToListenFor channel via AsyncGameplayMessageSystem. Returns Running.
 * When a matching message arrives, outputs the payload (as FArcAIGameplayAbilityEvent or instanced struct)
 * and fires OnResultChanged, signaling the entity.
 * Optionally auto-completes after Duration seconds.
 *
 * This is a latent event listener — it should be the primary or only task in its state,
 * and state transitions should be driven by the OnResultChanged delegate.
 */
USTRUCT(meta = (DisplayName = "Arc Mass Actor Listen To Async Message Task", Category = "Arc|Events", ToolTip = "Latent task that listens for async gameplay messages on the entity's actor. Fires OnResultChanged when a matching message arrives. Supports optional duration timeout. Should be the primary task in its state."))
struct FArcMassActorListenToAsyncMessageTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()
	
	using FInstanceDataType = FArcMassActorListenToAsyncMessageTaskInstanceData;

public:
	FArcMassActorListenToAsyncMessageTask();
	
protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;
	
	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

	TStateTreeExternalDataHandle<FMassActorFragment> MassActorFragment;
	TStateTreeExternalDataHandle<FArcMassAsyncMessageEndpointFragment> AsyncMessageEndpointFragment; 
};
