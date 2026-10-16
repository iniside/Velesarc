// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/Tasks/ArcEffectSTTask_WaitEffectApplied.h"

#include "NativeGameplayTags.h"
#include "StateTreeExecutionContext.h"
#include "Effects/ArcEffectStateTreeEvents.h"
#include "Effects/ArcEffectComponent.h"
#include "Events/ArcGameEventSubsystem.h"
#include "Events/ArcGameEventTags.h"
#include "Events/ArcEffectEventPayloads.h"
#include "Effects/ArcEffectSpec.h"
#include "MassStateTreeDependency.h"
#include "Mass/EntityHandle.h"
#include "Effects/ArcEffectStateTreeContext.h"
#include "Schema/ArcEffectStateTreeSchema.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcEffectSTTask_WaitEffectApplied)

void FArcEffectSTTask_WaitEffectApplied::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadWrite<UArcGameEventSubsystem>();
}

EStateTreeRunStatus FArcEffectSTTask_WaitEffectApplied::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FArcEffectSTTask_WaitEffectAppliedInstanceData& InstanceData = Context.GetInstanceData<FArcEffectSTTask_WaitEffectAppliedInstanceData>(*this);
	FArcEffectStateTreeContext* EffectContextPtr = Context.GetContextDataByName(Arcx::MassAbilities::Effect::EffectTreeContext)
		.GetMutablePtr<FArcEffectStateTreeContext>();
	const FMassEntityHandle* OwnerEntity = Context.GetContextDataByName(Arcx::MassAbilities::Effect::OwnerEntityHandle)
		.GetPtr<FMassEntityHandle>();

	UWorld* World = Context.GetWorld();
	UArcGameEventSubsystem* EventBus = World ? World->GetSubsystem<UArcGameEventSubsystem>() : nullptr;
	if (!EventBus || !EffectContextPtr || !OwnerEntity || !OwnerEntity->IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	UArcEffectDefinition* CapturedFilter = InstanceData.EffectFilter;
	FArcGameplayTagRequirements CapturedTagFilter = InstanceData.TagFilter;

	InstanceData.DelegateHandle = EventBus->Subscribe(ArcGameEventTags::EffectApplied, *OwnerEntity, EArcEventRole::Target,
		FArcGameEventDelegate::CreateLambda(
			[EffectContextPtr, CapturedFilter, CapturedTagFilter](const FArcGameEvent& Event)
			{
				const FArcEffectAppliedPayload* Applied = Event.Payload.GetPtr<FArcEffectAppliedPayload>();
				const FArcEffectSpec* Spec = Applied ? Applied->Spec.GetPtr<FArcEffectSpec>() : nullptr;
				UArcEffectDefinition* Effect = Spec ? Spec->Definition.Get() : nullptr;
				if (!Effect)
				{
					return;
				}

				if (CapturedFilter && Effect != CapturedFilter)
				{
					return;
				}

				if (!CapturedTagFilter.IsEmpty())
				{
					const FGameplayTagContainer& EffectTags = ArcEffectHelpers::GetEffectTags(Effect);
					if (!CapturedTagFilter.RequirementsMet(EffectTags))
					{
						return;
					}
				}

				FArcEffectEvent_EffectApplied Payload;
				Payload.Effect = Effect;
				Payload.SourceEntity = Event.Instigator;
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
		UWorld* World = Context.GetWorld();
		UArcGameEventSubsystem* EventBus = World ? World->GetSubsystem<UArcGameEventSubsystem>() : nullptr;
		if (EventBus)
		{
			EventBus->Unsubscribe(InstanceData.DelegateHandle);
		}
		InstanceData.DelegateHandle.Reset();
	}
}
