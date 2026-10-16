// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/Tasks/ArcEffectSTTask_WaitAttributeChanged.h"

#include "Engine/World.h"
#include "NativeGameplayTags.h"
#include "StateTreeExecutionContext.h"
#include "Effects/ArcEffectStateTreeEvents.h"
#include "Events/ArcGameEventSubsystem.h"
#include "Events/ArcAttributeEventPayloads.h"
#include "MassStateTreeDependency.h"
#include "Mass/EntityHandle.h"
#include "Effects/ArcEffectStateTreeContext.h"
#include "Schema/ArcEffectStateTreeSchema.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcEffectSTTask_WaitAttributeChanged)

void FArcEffectSTTask_WaitAttributeChanged::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadWrite<UArcGameEventSubsystem>();
}

EStateTreeRunStatus FArcEffectSTTask_WaitAttributeChanged::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FArcEffectSTTask_WaitAttributeChangedInstanceData& InstanceData =
		Context.GetInstanceData<FArcEffectSTTask_WaitAttributeChangedInstanceData>(*this);

	FArcEffectStateTreeContext* EffectContextPtr = Context.GetContextDataByName(Arcx::MassAbilities::Effect::EffectTreeContext)
		.GetMutablePtr<FArcEffectStateTreeContext>();
	const FMassEntityHandle* OwnerEntity = Context.GetContextDataByName(Arcx::MassAbilities::Effect::OwnerEntityHandle)
		.GetPtr<FMassEntityHandle>();

	UWorld* World = Context.GetWorld();
	UArcGameEventSubsystem* EventBus = World ? World->GetSubsystem<UArcGameEventSubsystem>() : nullptr;
	if (EventBus == nullptr || EffectContextPtr == nullptr || OwnerEntity == nullptr || !OwnerEntity->IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.DelegateHandle = EventBus->Subscribe(*OwnerEntity, InstanceData.Attribute,
		FArcGameEventDelegate::CreateLambda(
			[EffectContextPtr](const FArcGameEvent& Event)
			{
				const FArcAttributeChangedPayload* Changed = Event.Payload.GetPtr<FArcAttributeChangedPayload>();
				if (!Changed)
				{
					return;
				}
				FArcEffectEvent_AttributeChanged Payload;
				Payload.Attribute = Changed->Attribute;
				Payload.OldValue = Changed->OldValue;
				Payload.NewValue = Changed->NewValue;
				EffectContextPtr->RequestTick(
					ArcEffectStateTreeTags::AttributeChanged.GetTag(),
					FConstStructView::Make(Payload));
			}));

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcEffectSTTask_WaitAttributeChanged::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	bool bEventReceived = false;

	Context.ForEachEvent([&bEventReceived](const FStateTreeEvent& Event) -> EStateTreeLoopEvents
	{
		if (Event.Tag == ArcEffectStateTreeTags::AttributeChanged.GetTag())
		{
			bEventReceived = true;
			return EStateTreeLoopEvents::Break;
		}
		return EStateTreeLoopEvents::Next;
	});

	return bEventReceived ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}

void FArcEffectSTTask_WaitAttributeChanged::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FArcEffectSTTask_WaitAttributeChangedInstanceData& InstanceData =
		Context.GetInstanceData<FArcEffectSTTask_WaitAttributeChangedInstanceData>(*this);

	if (!InstanceData.DelegateHandle.IsValid())
	{
		return;
	}

	UWorld* World = Context.GetWorld();
	if (UArcGameEventSubsystem* EventBus = World ? World->GetSubsystem<UArcGameEventSubsystem>() : nullptr)
	{
		EventBus->Unsubscribe(InstanceData.DelegateHandle);
	}

	InstanceData.DelegateHandle.Reset();
}
