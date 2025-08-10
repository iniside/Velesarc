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



#include "ArcGunInstanceBase.h"

#include "GameplayEffectExecutionCalculation.h"

#include "ArcGunAttributeSet.h"
#include "ArcGunItemInstance.h"
#include "ArcGunStateComponent.h"

#include "AbilitySystem/ArcAbilitiesTypes.h"

#include "Fragments/ArcItemFragment_GunAmmoInfo.h"
#include "Fragments/ArcItemFragment_GunStats.h"

#include "Player/ArcCorePlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"

#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemData.h"

#include "Items/ArcItemsComponent.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/ArcItemsStoreComponent.h"
#include "Items/Fragments/ArcItemFragment_ItemStats.h"

DEFINE_LOG_CATEGORY(LogArcGunInstanceBase);

void FArcGunInstanceBase::Initialize(UArcGunStateComponent* InComponent)
{
	if (Character == nullptr)
	{
		Character = InComponent->GetOwner<APawn>();
		if (Character == nullptr)
		{
			APlayerState* PS = InComponent->GetOwner<APlayerState>();
			Character = PS->GetPawn<APawn>();
		}
	}
}

bool FArcGunInstanceBase::CommitAmmo(const FArcItemData* InItem, class UArcGunStateComponent* InComponent)
{
	UE_LOG(LogArcGunInstanceBase, Verbose, TEXT("CommitFireShot IsServer %s"), InComponent->GetNetMode() == NM_DedicatedServer ? TEXT("Server") : TEXT("Client"));
	UE_CLOG(InItem == nullptr, LogArcGunInstanceBase, Verbose, TEXT("FArcGunInstanceBase::CommitAmmo ItemInvalid"))
	UE_CLOG(InItem != nullptr, LogArcGunInstanceBase, Verbose, TEXT("FArcGunInstanceBase::CommitAmmo Item Valid"))
	
	float Cost = InItem->GetValue(ArcGunStats::GetAmmoCostData());

	// TODO: Replace with new instances
	FArcItemInstance_GunMagazineAmmo* Instance = ArcItems::FindMutableInstance<FArcItemInstance_GunMagazineAmmo>(InItem);
	if (Instance == nullptr)
	{
		return true;
	}
	
	int32 CurrentAmmo = GetMagazineAmmo(InItem, InComponent);
	
	int32 MaxAmmo = GetMaxMagazine(InItem, InComponent);
	
	int32 AmmoToRemove = FMath::Max(0, MaxAmmo - CurrentAmmo);
	
	if(Cost <= 0)
	{
		return true;
	}
	if (Cost > Instance->GetAmmo())
	{
		return false;
	}
	
	const int32 NewAmmo = CurrentAmmo - Cost;
	Instance->SetAmmo(NewAmmo);

	// TODO:: It should be more auomated on server/standalone.
	ArcItems::BroadcastItemChanged(InItem);
	InComponent->BroadcastOnWeaponAmmoChanged(Instance->GetAmmo());
	return true;
}

bool FArcGunInstanceBase::CanReload(const FArcItemData* InItem
	, UArcGunStateComponent* InComponent)
{
	const FArcItemFragment_GunAmmoInfo* WeaponAmmoInfo = InItem->GetItemDefinition()->FindFragment<FArcItemFragment_GunAmmoInfo>();
	if(WeaponAmmoInfo == nullptr)
	{
		return false;
	}
	
	if(WeaponAmmoInfo->AmmoItem == nullptr)
	{
		return false;
	}

	int32 Amount = InComponent->GetItemsComponent()->GetItemStacks(WeaponAmmoInfo->AmmoItem);
	if(Amount <= 0)
	{
		return false;
	}

	return true;
}

void FArcGunInstanceBase::Reload(const FArcItemData* InItem
	, UArcGunStateComponent* InComponent)
{
	FArcItemInstance_GunMagazineAmmo* Instance = ArcItems::FindMutableInstance<FArcItemInstance_GunMagazineAmmo>(InItem);
	if (Instance == nullptr)
	{
		return;
	}
	
	int32 CurrentAmmo = GetMagazineAmmo(InItem, InComponent);
	
	int32 MaxAmmo = GetMaxMagazine(InItem, InComponent);
	
	int32 AmmoToRemove = FMath::Max(0, MaxAmmo - CurrentAmmo);
	
	const FArcItemFragment_GunAmmoInfo* WeaponAmmoInfo = InItem->GetItemDefinition()->FindFragment<FArcItemFragment_GunAmmoInfo>();
	
	//const FArcItemData* AmmoItem = InComponent->GetItemsComponent()->GetItemsStore()->GetItemByDefinition(WeaponAmmoInfo->AmmoItem);
	//InComponent->GetItemsComponent()->RemoveItem(AmmoItem->GetItemId(), AmmoToRemove, false);
	
	Instance->SetAmmo(MaxAmmo);
	ArcItems::BroadcastItemChanged(InItem);
}

int32 FArcGunInstanceBase::GetMagazineAmmo(const FArcItemData* InItem
	, UArcGunStateComponent* InComponent)
{
	FArcItemInstance_GunMagazineAmmo* Instance = ArcItems::FindMutableInstance<FArcItemInstance_GunMagazineAmmo>(InItem);
	if (Instance == nullptr)
	{
		return 99999;
	}
	
	return Instance->GetAmmo();
}

int32 FArcGunInstanceBase::GetWeaponAmmo(const FArcItemData* InItem
	, UArcGunStateComponent* InComponent)
{
	
	const FArcItemFragment_GunAmmoInfo* WeaponAmmoInfo = InItem->GetItemDefinition()->FindFragment<FArcItemFragment_GunAmmoInfo>();
	if (WeaponAmmoInfo == nullptr)
	{
		return 0;
	}
	if (WeaponAmmoInfo->AmmoItem == nullptr)
	{
		return 0;
	}

	int32 Amount = InComponent->GetItemsComponent()->GetItemStacks(WeaponAmmoInfo->AmmoItem);
	return Amount;
}

float FArcGunInstanceBase::GetFireRate(const FArcItemData* InItem
                                                , UArcGunStateComponent* InComponent) const
{
	float ROF = InItem->GetValue(ArcGunStats::GetRoundsPerMinuteData());
	float Final = 1 / (ROF / 60);
	return Final;
}

float FArcGunInstanceBase::GetTriggerCooldown(const FArcItemData* InItem
	, UArcGunStateComponent* InComponent) const
{
	return InItem->GetValue(ArcGunStats::GetTriggerCooldownData());
}

float FArcGunInstanceBase::GetReloadTime(const FArcItemData* InItem
	, UArcGunStateComponent* InComponent) const
{
	const float Value = InItem->GetValue(ArcGunStats::GetReloadTimeData());
	return Value;
}

int32 FArcGunInstanceBase::GetMaxMagazine(const FArcItemData* InItem
                                                   , UArcGunStateComponent* InComponent) const
{
	return InItem->GetValue(ArcGunStats::GetBaseMagazineData());
}