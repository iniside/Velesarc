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

#include "ArcGunAttributeSet.h"
#include "Net/UnrealNetwork.h"

#include "GameplayEffectExtension.h"

#include "Items/ArcItemDefinition.h"
#include "ArcCore/Items/ArcItemData.h"
#include "ArcCore/Items/ArcItemsComponent.h"

#include "ArcCore/AbilitySystem/ArcCoreGameplayAbility.h"


void UArcGunAttributeSet::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	DOREPLIFETIME_CONDITION_NOTIFY(UArcGunAttributeSet, WeaponDamage, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArcGunAttributeSet, Accuracy, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArcGunAttributeSet, Stability, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArcGunAttributeSet, Handling, COND_OwnerOnly, REPNOTIFY_Always);
}

void UArcGunAttributeSet::OnRep_WeaponDamage(const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArcGunAttributeSet, WeaponDamage, Old.GetCurrentValue());
}

void UArcGunAttributeSet::OnRep_Accuracy(
    const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArcGunAttributeSet, Accuracy, Old.GetCurrentValue());
}

void UArcGunAttributeSet::OnRep_Stability(
    const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArcGunAttributeSet, Stability, Old.GetCurrentValue());
}

void UArcGunAttributeSet::OnRep_Handling(
	const FGameplayAttributeData& Old)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArcGunAttributeSet, Handling, Old.GetCurrentValue());
}

UArcGunAttributeSet::UArcGunAttributeSet()
{
	//WeaponDamage.SetBaseValue(1.0f);
	//HeadshotDamage.SetBaseValue(1.0f);
	//WeakpointDamage.SetBaseValue(1.0f);
	//CriticalDamage.SetBaseValue(1.0f);
	//CriticalHitChance.SetBaseValue(1.0f);
	//FireRate.SetBaseValue(1.0f);
	//ReloadSpeedBonus.SetBaseValue(1.0f);
	//MaxMagazine.SetBaseValue(1.0f);
	//Handling.SetBaseValue(1.0f);
	//Accuracy.SetBaseValue(1.0f);
	//Stability.SetBaseValue(1.0f);
	//Range.SetBaseValue(1.0f);

}