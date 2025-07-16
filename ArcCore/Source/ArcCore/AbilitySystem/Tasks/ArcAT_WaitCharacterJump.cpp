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

#include "ArcCore/AbilitySystem/Tasks/ArcAT_WaitCharacterJump.h"

#include "AbilitySystemComponent.h"
#include "ArcCore/AbilitySystem/ArcAbilityTargetingComponent.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "GameFramework/Character.h"

UArcAT_WaitCharacterJump* UArcAT_WaitCharacterJump::WaitCharacterJump(UGameplayAbility* OwningAbility
																	  , FName TaskInstanceName)
{
	UArcAT_WaitCharacterJump* Proxy = NewAbilityTask<UArcAT_WaitCharacterJump>(OwningAbility
		, TaskInstanceName);
	return Proxy;
}

void UArcAT_WaitCharacterJump::Activate()
{
	Super::Activate();

	ACharacter* C = Cast<ACharacter>(GetAvatarActor());

	if (!C)
	{
		UE_LOG(LogTemp
			, Warning
			, TEXT("UAuAT_WaitCharacterJump No Valid Character"));
		return;
	}
	C->OnReachedJumpApex.AddDynamic(this
		, &UArcAT_WaitCharacterJump::OnApex);
	C->LandedDelegate.AddDynamic(this
		, &UArcAT_WaitCharacterJump::Landed);
}

void UArcAT_WaitCharacterJump::OnDestroy(bool bInOwnerFinished)
{
	ACharacter* C = Cast<ACharacter>(GetAvatarActor());

	if (!C)
	{
		Super::OnDestroy(bInOwnerFinished);
		return;
	}

	C->OnReachedJumpApex.RemoveDynamic(this
		, &UArcAT_WaitCharacterJump::OnApex);
	C->LandedDelegate.RemoveDynamic(this
		, &UArcAT_WaitCharacterJump::Landed);

	Super::OnDestroy(bInOwnerFinished);
}

void UArcAT_WaitCharacterJump::OnApex()
{
	OnJumpApex.Broadcast();
}

void UArcAT_WaitCharacterJump::Landed(const FHitResult& HitResult)
{
	OnLanded.Broadcast();
}
