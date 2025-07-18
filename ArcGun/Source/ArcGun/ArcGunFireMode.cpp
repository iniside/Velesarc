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



#include "ArcGunFireMode.h"

#include "ArcGunStateComponent.h"

#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemData.h"

bool FArcGunFireMode_FullAuto::FireStart(const FGameplayAbilitySpecHandle& TargetRequest, UArcGunStateComponent* InGunComponent)
{
	FArcGunInstanceBase* Extension = InGunComponent->GetGunInstance<FArcGunInstanceBase>();
	ElapsedTime = 0;
	//something to consider is setting it up only on clients.
	float TriggerCooldown = Extension->GetTriggerCooldown(InGunComponent->GetSelectedGun().GunItemPtr
		, InGunComponent);

	if (InGunComponent->CommitAmmo() == false)
	{
		InGunComponent->StopFire();
		return false;
	}
	
	StartTime = InGunComponent->GetWorld()->GetTimeSeconds();
	ROF = Extension->GetFireRate(InGunComponent->GetSelectedGun().GunItemPtr, InGunComponent);

	return true;
}

void FArcGunFireMode_FullAuto::FireStop(const FGameplayAbilitySpecHandle& TargetRequest, UArcGunStateComponent* InGunComponent)
{
	
}

bool FArcGunFireMode_FullAuto::FireUpdate(const FGameplayAbilitySpecHandle& TargetRequest, UArcGunStateComponent* InGunComponent, float DeltaTime)
{
	float Scale = ShootScaleCurve != nullptr ? ShootScaleCurve->GetFloatValue(ShootCount) : 1.0f;
	
	CurrentFirstShootDelayTime += DeltaTime;
	if(CurrentFirstShootDelayTime >= ShootDelay)
	{
		float ScaledRPM = ROF * Scale;
		//float Time = InGunComponent->GetWorld()->GetTimeSeconds() - TimeLastShot;

		if(CurrentShootTime > ScaledRPM)
		{
			ElapsedTime = InGunComponent->GetWorld()->GetTimeSeconds() - StartTime;
			//UE_LOG(LogWaitWeaponAutoFire, Log, TEXT("TickTask [Time %f] [ROF %f], [CurrentShootTime %f]"), Time, ScaledRPM, CurrentShootTime);
			//UE_LOG(LogWaitWeaponAutoFire, Log, TEXT("TickTask [CurrentShootTime %f] - [ScaledRPM %f] - [DeltaTime %f]"), CurrentShootTime, ScaledRPM, DeltaTime);
			if (InGunComponent->CommitAmmo() == false)
			{
				InGunComponent->StopFire();
				return false;
			}
			
			CurrentShootTime = 0;
			
			return true;
		}
		
		CurrentShootTime += DeltaTime;

		return false;
	}
	
	return false;
}

bool FArcGunFireMode_Single::FireStart(const FGameplayAbilitySpecHandle& TargetRequest, UArcGunStateComponent* InGunComponent)
{
	FArcGunInstanceBase* Extension = InGunComponent->GetGunInstance<FArcGunInstanceBase>();
	float TriggerCooldown = Extension->GetTriggerCooldown(InGunComponent->GetSelectedGun().GunItemPtr, InGunComponent);

	ElapsedTime = 0;
	
	const double CurrentTime = InGunComponent->GetWorld()->GetTimeSeconds();
	const double TimeDelta = CurrentTime - StartTime; 

	if (TimeDelta < ROF)
	{
		InGunComponent->StopFire();
		return false;
	}
	
	if (InGunComponent->CommitAmmo() == false)
	{
		InGunComponent->StopFire();
		return false;
	}
		
	StartTime = InGunComponent->GetWorld()->GetTimeSeconds();
	ROF = Extension->GetFireRate(InGunComponent->GetSelectedGun().GunItemPtr, InGunComponent);
	
	return true;
}

void FArcGunFireMode_Single::FireStop(const FGameplayAbilitySpecHandle& TargetRequest, UArcGunStateComponent* InGunComponent)
{
	
}

bool FArcGunFireMode_Single::FireUpdate(const FGameplayAbilitySpecHandle& TargetRequest, UArcGunStateComponent* InGunComponent, float DeltaTime)
{
	InGunComponent->StopFire();
	return false;
}
