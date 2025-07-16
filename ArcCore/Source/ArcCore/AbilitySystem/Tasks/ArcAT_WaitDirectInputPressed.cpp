/**
 * This file is part of ArcX.
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

#include "ArcCore/AbilitySystem/Tasks/ArcAT_WaitDirectInputPressed.h"
#include "ArcCore/AbilitySystem/ArcCoreAbilitySystemComponent.h"

UArcAT_WaitDirectInputPressed* UArcAT_WaitDirectInputPressed::WaitDirectInputPressed(UGameplayAbility* OwningAbility
																					 , FName TaskInstanceName)
{
	UArcAT_WaitDirectInputPressed* MyObj = NewAbilityTask<UArcAT_WaitDirectInputPressed>(OwningAbility
		, TaskInstanceName);
	return MyObj;
}

void UArcAT_WaitDirectInputPressed::Activate()
{
	Super::Activate();
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);

	SetWaitingOnRemotePlayerData();
	SetWaitingOnAvatar();

	InputDelegateHandle = ArcASC->AddOnInputPressedMap(GetAbilitySpecHandle()
		, FArcAbilityInputDelegate::FDelegate::CreateUObject(this
			, &UArcAT_WaitDirectInputPressed::HandleOnInputPressed));
}

void UArcAT_WaitDirectInputPressed::HandleOnInputPressed(FGameplayAbilitySpec& InSpec)
{
	OnInputPressed.Broadcast(0.0f);
}
