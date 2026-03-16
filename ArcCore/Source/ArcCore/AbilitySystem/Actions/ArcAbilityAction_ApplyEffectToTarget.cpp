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

#include "ArcAbilityAction_ApplyEffectToTarget.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"

void FArcAbilityAction_ApplyEffectToTarget::Execute(FArcAbilityActionContext& Context)
{
	if (!GameplayEffectClass) { return; }

	UAbilitySystemComponent* SourceASC = Context.Ability->GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC) { return; }

	FGameplayEffectSpecHandle SpecHandle = Context.Ability->MakeOutgoingGameplayEffectSpec(
		Context.Handle,
		Context.ActorInfo,
		Context.ActivationInfo,
		GameplayEffectClass, Level);
	
	if (!SpecHandle.IsValid()) { return; }

	for (int32 i = 0; i < Context.TargetData.Num(); ++i)
	{
		const FGameplayAbilityTargetData* Data = Context.TargetData.Get(i);
		if (!Data) { continue; }

		TArray<TWeakObjectPtr<AActor>> Actors = Data->GetActors();
		for (const TWeakObjectPtr<AActor>& ActorPtr : Actors)
		{
			AActor* TargetActor = ActorPtr.Get();
			if (!TargetActor) { continue; }

			UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
			if (TargetASC)
			{
				SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
			}
		}
	}
}
