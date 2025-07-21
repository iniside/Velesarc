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

#include "ArcCore/AbilitySystem/Tasks/ArcAT_WaitDirectInputRelease.h"

#include "Engine/World.h"

#include "ArcCore/AbilitySystem/ArcCoreAbilitySystemComponent.h"

UArcAT_WaitDirectInputRelease* UArcAT_WaitDirectInputRelease::WaitDirectInputRelease(UGameplayAbility* OwningAbility
																					 , FName TaskInstanceName)
{
	UArcAT_WaitDirectInputRelease* MyObj = NewAbilityTask<UArcAT_WaitDirectInputRelease>(OwningAbility
		, TaskInstanceName);
	return MyObj;
}

void UArcAT_WaitDirectInputRelease::Activate()
{
	Super::Activate();
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);

	SetWaitingOnRemotePlayerData();
	SetWaitingOnAvatar();
	StartTime = GetWorld()->GetTime().GetWorldTimeSeconds();

	InputDelegateHandle = ArcASC->AddOnInputReleasedMap(GetAbilitySpecHandle()
		, FArcAbilityInputDelegate::FDelegate::CreateUObject(this
			, &UArcAT_WaitDirectInputRelease::HandleOnInputReleased));
}

void UArcAT_WaitDirectInputRelease::HandleOnInputReleased(FGameplayAbilitySpec& InSpec)
{
	const float Time = GetWorld()->GetTime().GetWorldTimeSeconds() - StartTime;
	OnInputReleased.Broadcast(Time);
}
