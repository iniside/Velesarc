/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "ArcCore/AbilitySystem/Tasks/ArcAT_RepeatUntilInputRelease.h"
#include "TimerManager.h"

#include "AbilitySystemComponent.h"

#include "Abilities/GameplayAbility.h"
#include "Engine/World.h"

UArcAT_RepeatUntilInputRelease::UArcAT_RepeatUntilInputRelease(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StartTime = 0.f;
	bTestInitialState = false;
	TimeBetweenTicks = 0;
}

void UArcAT_RepeatUntilInputRelease::OnReleaseCallback()
{
	float ElapsedTime = GetWorld()->GetTimeSeconds() - StartTime;

	if (!Ability || !AbilitySystemComponent.IsValid())
	{
		return;
	}

	AbilitySystemComponent->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::InputReleased
		, GetAbilitySpecHandle()
		, GetActivationPredictionKey()).Remove(DelegateHandle);

	FScopedPredictionWindow ScopedPrediction(AbilitySystemComponent.Get()
		, IsPredictingClient());

	if (IsPredictingClient())
	{
		// Tell the server about this
		AbilitySystemComponent->ServerSetReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased
			, GetAbilitySpecHandle()
			, GetActivationPredictionKey()
			, AbilitySystemComponent->ScopedPredictionKey);
	}
	else
	{
		AbilitySystemComponent->ConsumeGenericReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased
			, GetAbilitySpecHandle()
			, GetActivationPredictionKey());
	}

	// We are done. Kill us so we don't keep getting broadcast messages
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnRelease.Broadcast(ElapsedTime);
	}
	EndTask();
}

void UArcAT_RepeatUntilInputRelease::OnTickCallback()
{
	float ElapsedTime = GetWorld()->GetTimeSeconds() - StartTime;
	OnTick.Broadcast(ElapsedTime);
}

UArcAT_RepeatUntilInputRelease* UArcAT_RepeatUntilInputRelease::WaitRepeatUntilInputRelease(
	class UGameplayAbility* OwningAbility
	, float InTimeBetweenTicks
	, bool bTestAlreadyReleased)
{
	UArcAT_RepeatUntilInputRelease* Task = NewAbilityTask<UArcAT_RepeatUntilInputRelease>(OwningAbility);
	Task->TimeBetweenTicks = InTimeBetweenTicks;
	Task->bTestInitialState = bTestAlreadyReleased;
	return Task;
}

void UArcAT_RepeatUntilInputRelease::Activate()
{
	StartTime = GetWorld()->GetTimeSeconds();
	if (Ability)
	{
		if (bTestInitialState && IsLocallyControlled())
		{
			FGameplayAbilitySpec* Spec = Ability->GetCurrentAbilitySpec();
			if (Spec && !Spec->InputPressed)
			{
				OnReleaseCallback();
				return;
			}
		}

		DelegateHandle = AbilitySystemComponent->AbilityReplicatedEventDelegate(
			EAbilityGenericReplicatedEvent::InputReleased
			, GetAbilitySpecHandle()
			, GetActivationPredictionKey()).AddUObject(this
			, &UArcAT_RepeatUntilInputRelease::OnReleaseCallback);
		if (IsForRemoteClient())
		{
			if (!AbilitySystemComponent->CallReplicatedEventDelegateIfSet(EAbilityGenericReplicatedEvent::InputReleased
				, GetAbilitySpecHandle()
				, GetActivationPredictionKey()))
			{
				SetWaitingOnRemotePlayerData();
			}
		}
	}

	if (TimeBetweenTicks <= 0)
	{
		return;
	}

	FTimerManager& Timer = AbilitySystemComponent->GetOwner()->GetWorldTimerManager();

	FTimerDelegate RepeateDelegate = FTimerDelegate::CreateUObject(this
		, &UArcAT_RepeatUntilInputRelease::OnTickCallback);

	Timer.SetTimer(RepeatHandle
		, RepeateDelegate
		, TimeBetweenTicks
		, true);
}

void UArcAT_RepeatUntilInputRelease::OnDestroy(bool bInOwnerFinished)
{
	FTimerManager& Timer = AbilitySystemComponent->GetOwner()->GetWorldTimerManager();
	Timer.ClearTimer(RepeatHandle);
	Super::OnDestroy(bInOwnerFinished);
}
