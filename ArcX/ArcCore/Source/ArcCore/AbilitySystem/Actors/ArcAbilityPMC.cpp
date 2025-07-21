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

#include "ArcCore/AbilitySystem/Actors/ArcAbilityPMC.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"

UArcAbilityPMC::UArcAbilityPMC()
{
	bWantsInitializeComponent = true;
}

void UArcAbilityPMC::ApplyEffect(AActor* Instigator
								 , FGameplayEffectSpecHandle EffectSpec)
{
	if (!EffectSpec.IsValid())
	{
		return;
	}

	FGameplayEffectContextHandle Ctx = EffectSpec.Data->GetContext().Duplicate();
	Ctx.Get()->SetEffectCauser(Cast<AActor>(GetOuter()));

	EffectSpec.Data->SetContext(Ctx);

	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Instigator))
	{
		ASI->GetAbilitySystemComponent()->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());
		return;
	}

	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(GetOuter()))
	{
		ASI->GetAbilitySystemComponent()->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());
		return;
	}
}

void UArcAbilityPMC::InitializeComponent()
{
	Super::InitializeComponent();
}
