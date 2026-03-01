// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "ArcKnowledgeEvent.h"
#include "AsyncMessageHandle.h"
#include "AsyncMessageId.h"
#include "GameplayTagContainer.h"
#include "StateTreeDelegate.h"
#include "ArcMassListenToKnowledgeEventTask.generated.h"

/**
 * Instance data for the knowledge event listener task.
 */
USTRUCT()
struct FArcMassListenToKnowledgeEventTaskInstanceData
{
	GENERATED_BODY()

	// --- Parameters ---

	/** Which async message ID to listen on. Typically the Spatial or Global message ID from the broadcaster config. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FAsyncMessageId MessageId = FAsyncMessageId::Invalid;

	/** If set, only accept events whose tags match this query. Empty = accept all. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagQuery TagFilter;

	/** If set, only accept events of these types. Empty = accept all. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TArray<EArcKnowledgeEventType> AcceptedEventTypes;

	// --- Outputs ---

	/** The most recently received knowledge event. */
	UPROPERTY(EditAnywhere, Category = Output)
	FArcKnowledgeChangedEvent ReceivedEvent;

	/** Whether an event has been received since EnterState. */
	UPROPERTY(EditAnywhere, Category = Output)
	bool bEventReceived = false;

	/** Delegate dispatched when a new event is received. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnEventReceived;

	// --- Internal ---

	FAsyncMessageHandle BoundListenerHandle = FAsyncMessageHandle::Invalid;
};

/**
 * StateTree running task that listens for knowledge change events
 * on the entity's own async message endpoint.
 *
 * Binds in Tick (once the endpoint fragment exists), stores the event
 * in instance data, fires a delegate, and signals NewStateTreeTaskRequired
 * so the StateTree can react immediately.
 *
 * For global events, use the GlobalMessageId from the broadcaster config.
 * For spatial (per-entity) events, use the SpatialMessageId.
 */
USTRUCT(meta = (DisplayName = "Arc Listen To Knowledge Event", Category = "ArcKnowledge"))
struct ARCKNOWLEDGE_API FArcMassListenToKnowledgeEventTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassListenToKnowledgeEventTaskInstanceData;

	FArcMassListenToKnowledgeEventTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const override;

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Event"); }
	virtual FColor GetIconColor() const override { return FColor(200, 160, 60); }
#endif
};
