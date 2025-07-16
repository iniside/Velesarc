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



#include "ArcAT_WaitObserverGunPreShot.h"

#include "ArcGunStateComponent.h"

void UArcAT_WaitObserverGunPreShot::OnDestroy(bool bInOwnerFinished)
{
	Super::OnDestroy(bInOwnerFinished);

	UArcGunStateComponent* GunComponent = UArcGunStateComponent::FindGunStateComponent(GetOwnerActor());

	GunComponent->RemoveOnPreShoot(GunPreShotDelegateHandle);
}

void UArcAT_WaitObserverGunPreShot::Activate()
{
	Super::Activate();

	UArcGunStateComponent* GunComponent = UArcGunStateComponent::FindGunStateComponent(GetOwnerActor());

	GunPreShotDelegateHandle = GunComponent->AddOnPreShoot(FArcOnShootFired::FDelegate::CreateUObject(
	this, &UArcAT_WaitObserverGunPreShot::HandleOnShootFired));
}

void UArcAT_WaitObserverGunPreShot::HandleOnShootFired(const FArcGunShotInfo& ShotInfo)
{
	OnPreShot.Broadcast(ShotInfo.TimeHeld, ShotInfo.bEnded, ShotInfo.Targets);
}

UArcAT_WaitObserverGunPreShot* UArcAT_WaitObserverGunPreShot::WaitObserverGunPreShot(
	UGameplayAbility* OwningAbility
	, FName TaskInstanceName
)
{	
	UArcAT_WaitObserverGunPreShot* Proxy = NewAbilityTask<UArcAT_WaitObserverGunPreShot>(OwningAbility, TaskInstanceName);
	return Proxy;
}
