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

#include "ArcCore/AbilitySystem/Tasks/ArcAT_WaitClientDelay.h"
#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"

UArcAT_WaitClientDelay* UArcAT_WaitClientDelay::WaitClientDelay(UGameplayAbility* OwningAbility
																, FName TaskInstanceName
																, float Delay)
{
	UArcAT_WaitClientDelay* Proxy = NewAbilityTask<UArcAT_WaitClientDelay>(OwningAbility
		, TaskInstanceName);
	Proxy->Duration = Delay;
	return Proxy;
}

void UArcAT_WaitClientDelay::Activate()
{
	UWorld* World = GetWorld();
	ENetMode NM = GetOwnerActor()->GetNetMode();
	ENetRole Role = GetOwnerActor()->GetLocalRole();

	if (NM == ENetMode::NM_DedicatedServer)
	{
		ServerStartTime = World->GetTimeSeconds();
	}
	if (Role < ENetRole::ROLE_Authority || NM == ENetMode::NM_Standalone)
	{
		// Use a dummy timer handle as we don't need to store it for later but we don't
		// need to look for something to clear
		FTimerHandle TimerHandle;
		World->GetTimerManager().SetTimer(TimerHandle
			, this
			, &UArcAT_WaitClientDelay::OnDelayFinished
			, Duration
			, false);
		ClientActivated.Broadcast();
	}
	CallOrAddReplicatedDelegate(EAbilityGenericReplicatedEvent::GenericSignalFromClient
		, FSimpleMulticastDelegate::FDelegate::CreateUObject(this
			, &UArcAT_WaitClientDelay::OnSignalCallback));
	Super::Activate();
}

void UArcAT_WaitClientDelay::OnDestroy(bool bInOwnerFinished)
{
	Super::OnDestroy(bInOwnerFinished);
}

void UArcAT_WaitClientDelay::OnDelayFinished()
{
	UE_LOG(LogTemp
		, Log
		, TEXT("Client Nx Delay Finished"));
	AbilitySystemComponent->ServerSetReplicatedEvent(EAbilityGenericReplicatedEvent::GenericSignalFromClient
		, GetAbilitySpecHandle()
		, GetActivationPredictionKey()
		, AbilitySystemComponent->ScopedPredictionKey);
	OnFinish.Broadcast();
}

void UArcAT_WaitClientDelay::OnSignalCallback()
{
	// maybe use cycles.. Don't trust floats;
	float CurrentTime = GetWorld()->GetTimeSeconds();
	float TimeDiff = CurrentTime - ServerStartTime;
	// probabaly should be something clever like latency.
	const float Tolerance = 0.15;
	if (TimeDiff >= (Duration - Tolerance))
	{
		UE_LOG(LogTemp
			, Log
			, TEXT("Server Nx Delay Finished"));
		OnFinish.Broadcast();
	}
	else
	{
		UE_LOG(LogTemp
			, Log
			, TEXT("Server Nx Delay Failed"));
		OnServerTimeMismatch.Broadcast();
	}
}
