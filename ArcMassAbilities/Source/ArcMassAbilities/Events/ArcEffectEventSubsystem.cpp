// Copyright Lukasz Baran. All Rights Reserved.

#include "Events/ArcEffectEventSubsystem.h"

FDelegateHandle UArcEffectEventSubsystem::SubscribeAnyAttributeChanged(FMassEntityHandle Entity, FArcOnAnyAttributeChanged::FDelegate Delegate)
{
	return AnyAttributeChangedDelegates.FindOrAdd(Entity).Add(MoveTemp(Delegate));
}

void UArcEffectEventSubsystem::UnsubscribeAnyAttributeChanged(FMassEntityHandle Entity, FDelegateHandle Handle)
{
	if (FArcOnAnyAttributeChanged* Delegate = AnyAttributeChangedDelegates.Find(Entity))
	{
		Delegate->Remove(Handle);
	}
}

FDelegateHandle UArcEffectEventSubsystem::SubscribeAttributeChanged(FMassEntityHandle Entity, FArcAttributeRef Attribute, FArcOnAttributeChanged::FDelegate Delegate)
{
	FArcAttributeSubscriptionKey Key{Entity, Attribute};
	return SpecificAttributeChangedDelegates.FindOrAdd(Key).Add(MoveTemp(Delegate));
}

void UArcEffectEventSubsystem::UnsubscribeAttributeChanged(FMassEntityHandle Entity, FArcAttributeRef Attribute, FDelegateHandle Handle)
{
	FArcAttributeSubscriptionKey Key{Entity, Attribute};
	if (FArcOnAttributeChanged* Delegate = SpecificAttributeChangedDelegates.Find(Key))
	{
		Delegate->Remove(Handle);
	}
}

FDelegateHandle UArcEffectEventSubsystem::SubscribeAbilityActivated(FMassEntityHandle Entity, FArcOnAbilityActivated::FDelegate Delegate)
{
	return AbilityActivatedDelegates.FindOrAdd(Entity).Add(MoveTemp(Delegate));
}

void UArcEffectEventSubsystem::UnsubscribeAbilityActivated(FMassEntityHandle Entity, FDelegateHandle Handle)
{
	if (FArcOnAbilityActivated* Delegate = AbilityActivatedDelegates.Find(Entity))
	{
		Delegate->Remove(Handle);
	}
}

FDelegateHandle UArcEffectEventSubsystem::SubscribeEffectApplied(FMassEntityHandle Entity, FArcOnEffectApplied::FDelegate Delegate)
{
	return EffectAppliedDelegates.FindOrAdd(Entity).Add(MoveTemp(Delegate));
}

void UArcEffectEventSubsystem::UnsubscribeEffectApplied(FMassEntityHandle Entity, FDelegateHandle Handle)
{
	if (FArcOnEffectApplied* Delegate = EffectAppliedDelegates.Find(Entity))
	{
		Delegate->Remove(Handle);
	}
}

FDelegateHandle UArcEffectEventSubsystem::SubscribeEffectExecuted(FMassEntityHandle Entity, FArcOnEffectExecuted::FDelegate Delegate)
{
	return EffectExecutedDelegates.FindOrAdd(Entity).Add(MoveTemp(Delegate));
}

void UArcEffectEventSubsystem::UnsubscribeEffectExecuted(FMassEntityHandle Entity, FDelegateHandle Handle)
{
	if (FArcOnEffectExecuted* Delegate = EffectExecutedDelegates.Find(Entity))
	{
		Delegate->Remove(Handle);
	}
}

void UArcEffectEventSubsystem::BroadcastAttributeChanged(FMassEntityHandle Entity, FArcAttributeRef Attribute, float OldValue, float NewValue)
{
	if (FArcOnAnyAttributeChanged* Delegate = AnyAttributeChangedDelegates.Find(Entity))
	{
		Delegate->Broadcast(Entity, Attribute, OldValue, NewValue);
	}

	FArcAttributeSubscriptionKey Key{Entity, Attribute};
	if (FArcOnAttributeChanged* Delegate = SpecificAttributeChangedDelegates.Find(Key))
	{
		Delegate->Broadcast(Entity, OldValue, NewValue);
	}
}

void UArcEffectEventSubsystem::BroadcastAbilityActivated(FMassEntityHandle Entity, UArcAbilityDefinition* Ability)
{
	if (FArcOnAbilityActivated* Delegate = AbilityActivatedDelegates.Find(Entity))
	{
		Delegate->Broadcast(Entity, Ability);
	}
}

void UArcEffectEventSubsystem::BroadcastEffectApplied(FMassEntityHandle Entity, UArcEffectDefinition* Effect)
{
	if (FArcOnEffectApplied* Delegate = EffectAppliedDelegates.Find(Entity))
	{
		Delegate->Broadcast(Entity, Effect);
	}
}

void UArcEffectEventSubsystem::BroadcastEffectExecuted(FMassEntityHandle Entity, UArcEffectDefinition* Effect)
{
	if (FArcOnEffectExecuted* Delegate = EffectExecutedDelegates.Find(Entity))
	{
		Delegate->Broadcast(Entity, Effect);
	}
}

FDelegateHandle UArcEffectEventSubsystem::SubscribeTagCountChanged(FMassEntityHandle Entity, FArcOnTagCountChanged::FDelegate Delegate)
{
	return TagCountChangedDelegates.FindOrAdd(Entity).Add(MoveTemp(Delegate));
}

void UArcEffectEventSubsystem::UnsubscribeTagCountChanged(FMassEntityHandle Entity, FDelegateHandle Handle)
{
	if (FArcOnTagCountChanged* Delegate = TagCountChangedDelegates.Find(Entity))
	{
		Delegate->Remove(Handle);
	}
}

void UArcEffectEventSubsystem::BroadcastTagCountChanged(FMassEntityHandle Entity, FGameplayTag Tag, int32 OldCount, int32 NewCount)
{
	if (FArcOnTagCountChanged* Delegate = TagCountChangedDelegates.Find(Entity))
	{
		Delegate->Broadcast(Entity, Tag, OldCount, NewCount);
	}
}

void UArcEffectEventSubsystem::RemoveEntity(FMassEntityHandle Entity)
{
	AnyAttributeChangedDelegates.Remove(Entity);
	AbilityActivatedDelegates.Remove(Entity);
	EffectAppliedDelegates.Remove(Entity);
	EffectExecutedDelegates.Remove(Entity);
	TagCountChangedDelegates.Remove(Entity);

	TArray<FArcAttributeSubscriptionKey> KeysToRemove;
	for (const TPair<FArcAttributeSubscriptionKey, FArcOnAttributeChanged>& Pair : SpecificAttributeChangedDelegates)
	{
		if (Pair.Key.Entity == Entity)
		{
			KeysToRemove.Add(Pair.Key);
		}
	}
	for (const FArcAttributeSubscriptionKey& Key : KeysToRemove)
	{
		SpecificAttributeChangedDelegates.Remove(Key);
	}
}
