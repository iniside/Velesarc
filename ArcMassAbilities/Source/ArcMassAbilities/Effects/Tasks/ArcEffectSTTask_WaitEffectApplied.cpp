// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/Tasks/ArcEffectSTTask_WaitEffectApplied.h"

#include "NativeGameplayTags.h"
#include "StateTreeExecutionContext.h"
#include "Effects/ArcEffectStateTreeEvents.h"
#include "Effects/ArcEffectComponent.h"
#include "Events/ArcEffectEventSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcEffectSTTask_WaitEffectApplied)

EStateTreeRunStatus FArcEffectSTTask_WaitEffectApplied::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FArcEffectSTTask_WaitEffectAppliedInstanceData& InstanceData = Context.GetInstanceData<FArcEffectSTTask_WaitEffectAppliedInstanceData>(*this);
	FArcEffectStateTreeContext& EffectContext = Context.GetExternalData(EffectContextHandle);
	const FMassEntityHandle& OwnerEntity = Context.GetExternalData(OwnerEntityHandle);

	UWorld* World = Context.GetWorld();
	UArcEffectEventSubsystem* EventSys = World ? World->GetSubsystem<UArcEffectEventSubsystem>() : nullptr;
	if (!EventSys || !OwnerEntity.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	FArcEffectStateTreeContext* EffectContextPtr = &EffectContext;
	UArcEffectDefinition* CapturedFilter = InstanceData.EffectFilter;
	FGameplayTagRequirements CapturedTagFilter = InstanceData.TagFilter;

	InstanceData.DelegateHandle = EventSys->SubscribeEffectApplied(OwnerEntity,
		FArcOnEffectApplied::FDelegate::CreateLambda(
			[EffectContextPtr, CapturedFilter, CapturedTagFilter](FMassEntityHandle Entity, UArcEffectDefinition* Effect)
			{
				// Check specific effect filter
				if (CapturedFilter && Effect != CapturedFilter)
				{
					return;
				}

				// Check tag filter
				if (!CapturedTagFilter.IsEmpty())
				{
					FGameplayTagContainer EffectTags = ArcEffectHelpers::GetEffectTags(Effect);
					if (!CapturedTagFilter.RequirementsMet(EffectTags))
					{
						return;
					}
				}

				FArcEffectEvent_EffectApplied Payload;
				Payload.Effect = Effect;
				EffectContextPtr->RequestTick(ArcEffectStateTreeTags::EffectApplied, FConstStructView::Make(Payload));
			}));

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcEffectSTTask_WaitEffectApplied::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	bool bEventReceived = false;
	Context.ForEachEvent([&bEventReceived](const FStateTreeEvent& Event)
	{
		if (Event.Tag == ArcEffectStateTreeTags::EffectApplied.GetTag())
		{
			bEventReceived = true;
			return EStateTreeLoopEvents::Break;
		}
		return EStateTreeLoopEvents::Next;
	});
	return bEventReceived ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}

void FArcEffectSTTask_WaitEffectApplied::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FArcEffectSTTask_WaitEffectAppliedInstanceData& InstanceData = Context.GetInstanceData<FArcEffectSTTask_WaitEffectAppliedInstanceData>(*this);
	if (InstanceData.DelegateHandle.IsValid())
	{
		const FMassEntityHandle& OwnerEntity = Context.GetExternalData(OwnerEntityHandle);
		UWorld* World = Context.GetWorld();
		UArcEffectEventSubsystem* EventSys = World ? World->GetSubsystem<UArcEffectEventSubsystem>() : nullptr;
		if (EventSys)
		{
			EventSys->UnsubscribeEffectApplied(OwnerEntity, InstanceData.DelegateHandle);
		}
		InstanceData.DelegateHandle.Reset();
	}
}
