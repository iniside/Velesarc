// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/Tasks/ArcEffectSTTask_WaitAttributeChanged.h"

#include "Engine/World.h"
#include "NativeGameplayTags.h"
#include "StateTreeExecutionContext.h"
#include "Effects/ArcEffectStateTreeEvents.h"
#include "Events/ArcEffectEventSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcEffectSTTask_WaitAttributeChanged)

EStateTreeRunStatus FArcEffectSTTask_WaitAttributeChanged::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FArcEffectSTTask_WaitAttributeChangedInstanceData& InstanceData =
		Context.GetInstanceData<FArcEffectSTTask_WaitAttributeChangedInstanceData>(*this);

	FArcEffectStateTreeContext& EffectContext = Context.GetExternalData(EffectContextHandle);
	const FMassEntityHandle& OwnerEntity = Context.GetExternalData(OwnerEntityHandle);

	UWorld* World = Context.GetWorld();
	UArcEffectEventSubsystem* EventSys = World ? World->GetSubsystem<UArcEffectEventSubsystem>() : nullptr;
	if (EventSys == nullptr || !OwnerEntity.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	FArcEffectStateTreeContext* EffectContextPtr = &EffectContext;
	const FArcAttributeRef CapturedAttribute = InstanceData.Attribute;

	InstanceData.DelegateHandle = EventSys->SubscribeAttributeChanged(
		OwnerEntity,
		InstanceData.Attribute,
		FArcOnAttributeChanged::FDelegate::CreateLambda(
			[EffectContextPtr, CapturedAttribute](FMassEntityHandle Entity, float OldValue, float NewValue)
			{
				FArcEffectEvent_AttributeChanged Payload;
				Payload.Attribute = CapturedAttribute;
				Payload.OldValue = OldValue;
				Payload.NewValue = NewValue;
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

	const FMassEntityHandle& OwnerEntity = Context.GetExternalData(OwnerEntityHandle);
	UWorld* World = Context.GetWorld();
	UArcEffectEventSubsystem* EventSys = World ? World->GetSubsystem<UArcEffectEventSubsystem>() : nullptr;
	if (EventSys != nullptr)
	{
		EventSys->UnsubscribeAttributeChanged(OwnerEntity, InstanceData.Attribute, InstanceData.DelegateHandle);
	}

	InstanceData.DelegateHandle.Reset();
}
