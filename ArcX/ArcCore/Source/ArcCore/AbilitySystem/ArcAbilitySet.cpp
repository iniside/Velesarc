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

#include "ArcCore/AbilitySystem/ArcAbilitySet.h"

#include "ArcCore/AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "ArcCore/AbilitySystem/ArcCoreGameplayAbility.h"

DEFINE_LOG_CATEGORY(LogArcAbilitySet);

void FArcAbilitySet_GrantedHandles::AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle)
{
	if (Handle.IsValid())
	{
		AbilitySpecHandles.Add(Handle);
	}
}

void FArcAbilitySet_GrantedHandles::AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle)
{
	if (Handle.IsValid())
	{
		GameplayEffectHandles.Add(Handle);
	}
}

void FArcAbilitySet_GrantedHandles::AddAttributeSet(UAttributeSet* Set)
{
	GrantedAttributeSets.Add(Set);
}

void FArcAbilitySet_GrantedHandles::TakeFromAbilitySystem(UArcCoreAbilitySystemComponent* ArcASC)
{
	check(ArcASC);

	if (!ArcASC->IsOwnerActorAuthoritative())
	{
		// Must be authoritative to give or take ability sets.
		return;
	}

	for (const FGameplayAbilitySpecHandle& Handle : AbilitySpecHandles)
	{
		if (Handle.IsValid())
		{
			ArcASC->ClearAbility(Handle);
		}
	}

	for (const FActiveGameplayEffectHandle& Handle : GameplayEffectHandles)
	{
		if (Handle.IsValid())
		{
			ArcASC->RemoveActiveGameplayEffect(Handle);
		}
	}

	for (UAttributeSet* Set : GrantedAttributeSets)
	{
		ArcASC->RemoveSpawnedAttribute(Set);
	}

	AbilitySpecHandles.Reset();
	GameplayEffectHandles.Reset();
	GrantedAttributeSets.Reset();
}

UArcAbilitySet::UArcAbilitySet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UArcAbilitySet::GiveToAbilitySystem(UArcCoreAbilitySystemComponent* ArcASC
										 , FArcAbilitySet_GrantedHandles* OutGrantedHandles
										 , UObject* SourceObject) const
{
	check(ArcASC);

	ArcASC->GiveGlobalTargetingPreset(GlobalTargetingPresets);
	
	if (!ArcASC->IsOwnerActorAuthoritative())
	{
		// Must be authoritative to give or take ability sets.
		return;
	}

	// Grant the attribute sets.
	for (int32 SetIndex = 0; SetIndex < GrantedAttributes.Num(); ++SetIndex)
	{
		const FArcAbilitySet_AttributeSet& SetToGrant = GrantedAttributes[SetIndex];

		if (!SetToGrant.AttributeSet)
		{
			UE_LOG(LogArcAbilitySet
				, Error
				, TEXT("GrantedAttributes[%d] on ability set [%s] is not valid")
				, SetIndex
				, *GetNameSafe(this));
			continue;
		}

		UAttributeSet* NewSet = NewObject<UAttributeSet>(ArcASC->GetOwner()
			, SetToGrant.AttributeSet);
		ArcASC->AddAttributeSetSubobject(NewSet);
		ArcASC->InitStats(SetToGrant.AttributeSet
			, SetToGrant.AttributeMetaData);
		if (OutGrantedHandles)
		{
			OutGrantedHandles->AddAttributeSet(NewSet);
		}
	}
	ArcASC->InitializeAttributeSets();

	ArcASC->SetComponentTickEnabled(true);
	ArcASC->GetShouldTick();
	
	// Grant the gameplay abilities.
	for (int32 AbilityIndex = 0; AbilityIndex < GrantedGameplayAbilities.Num(); ++AbilityIndex)
	{
		const FArcAbilitySet_GameplayAbility& AbilityToGrant = GrantedGameplayAbilities[AbilityIndex];

		if (!AbilityToGrant.Ability)
		{
			UE_LOG(LogArcAbilitySet
				, Error
				, TEXT("GrantedGameplayAbilities [%d] on ability set [%s] is not valid.")
				, AbilityIndex
				, *GetNameSafe(this));
			continue;
		}

		UGameplayAbility* AbilityCDO = AbilityToGrant.Ability->GetDefaultObject<UGameplayAbility>();

		FGameplayAbilitySpec AbilitySpec(AbilityCDO
			, AbilityToGrant.AbilityLevel);
		AbilitySpec.SourceObject = SourceObject;
		AbilitySpec.GetDynamicSpecSourceTags().AddTag(AbilityToGrant.InputTag);

		const FGameplayAbilitySpecHandle AbilitySpecHandle = ArcASC->GiveAbility(AbilitySpec);

		if (AbilityToGrant.bAutoActivate)
		{
			ArcASC->TryActivateAbility(AbilitySpecHandle);
		}
		
		if (OutGrantedHandles)
		{
			OutGrantedHandles->AddAbilitySpecHandle(AbilitySpecHandle);
		}
	}

	// Grant the gameplay effects.
	for (int32 EffectIndex = 0; EffectIndex < GrantedGameplayEffects.Num(); ++EffectIndex)
	{
		const FArcAbilitySet_GameplayEffect& EffectToGrant = GrantedGameplayEffects[EffectIndex];

		if (!EffectToGrant.GameplayEffect)
		{
			UE_LOG(LogArcAbilitySet
				, Error
				, TEXT("GrantedGameplayEffects[%d] on ability set [%s] is not valid")
				, EffectIndex
				, *GetNameSafe(this));
			continue;
		}

		const UGameplayEffect* GameplayEffect = EffectToGrant.GameplayEffect->GetDefaultObject<UGameplayEffect>();
		const FActiveGameplayEffectHandle GameplayEffectHandle = ArcASC->ApplyGameplayEffectToSelf(GameplayEffect
			, EffectToGrant.EffectLevel
			, ArcASC->MakeEffectContext());

		if (OutGrantedHandles)
		{
			OutGrantedHandles->AddGameplayEffectHandle(GameplayEffectHandle);
		}
	}
}
