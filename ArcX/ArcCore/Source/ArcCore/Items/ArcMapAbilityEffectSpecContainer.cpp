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

#include "ArcMapAbilityEffectSpecContainer.h"

#include "AbilitySystemGlobals.h"
#include "AbilitySystemInterface.h"
#include "ArcGameplayEffectContext.h"
#include "ArcItemsComponent.h"
#include "ArcItemsStoreComponent.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Fragments/ArcItemFragment_AbilityEffectsToApply.h"

void FArcMapAbilityEffectSpecContainer::AddAbilityEffectSpecs(const FArcItemFragment_AbilityEffectsToApply* InContainer
															  , const FArcItemId& ItemHandle
															  , UArcItemsStoreComponent* ItemsComponent
															  , FArcMapAbilityEffectSpecContainer& InOutContainer)
{
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(ItemsComponent->GetOwner());
	if (!ASI)
	{
		return;
	}
	
	// Create a context for this ability
	FGameplayEffectContextHandle Context = FGameplayEffectContextHandle(UAbilitySystemGlobals::Get().AllocGameplayEffectContext());

	
	
	UArcCoreAbilitySystemComponent* ASC = Cast<UArcCoreAbilitySystemComponent>(ASI->GetAbilitySystemComponent());
	// By default use the owner and avatar as the instigator and causer
	FArcGameplayEffectContext* CtxPtr = static_cast<FArcGameplayEffectContext*>(Context.Get());

	CtxPtr->AddSourceObject(ItemsComponent);

	CtxPtr->SetSourceItemHandle(ItemHandle);

	Context.AddInstigator(ASC->AbilityActorInfo->OwnerActor.Get()
		, ASC->AbilityActorInfo->AvatarActor.Get());

	for (const TPair<FGameplayTag, FArcMapEffectItem>& EI : InContainer->Effects)
	{
		FArcEffectSpecItem SpecItem;
		SpecItem.ItemId = ItemHandle;
		SpecItem.SpecTag = EI.Key;
		SpecItem.SourceRequiredTags = EI.Value.SourceRequiredTags;
		SpecItem.SourceIgnoreTags = EI.Value.SourceIgnoreTags;
		SpecItem.TargetRequiredTags = EI.Value.TargetRequiredTags;
		SpecItem.TargetIgnoreTags = EI.Value.TargetIgnoreTags;

		for (const TSubclassOf<UGameplayEffect>& E : EI.Value.Effects)
		{
			FGameplayEffectSpecHandle Handle = ASC->MakeOutgoingSpec(E
				, 1.0f
				, Context);

			SpecItem.Specs.Add(Handle);
		}
		InOutContainer.SpecsArray.Add(SpecItem);
	}
}

void FArcMapAbilityEffectSpecContainer::RemoveAbilityEffectSpecs(FArcItemId ItemHandle
											 , FArcMapAbilityEffectSpecContainer& InOutContainer)
{
	int32 Idx = InOutContainer.SpecsArray.IndexOfByKey(ItemHandle);
	if (Idx != INDEX_NONE)
	{
		InOutContainer.SpecsArray.RemoveAt(Idx);
	}
}
