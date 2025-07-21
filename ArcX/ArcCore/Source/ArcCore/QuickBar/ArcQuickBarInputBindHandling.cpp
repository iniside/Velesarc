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



#include "QuickBar/ArcQuickBarInputBindHandling.h"

#include "ArcQuickBarComponent.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/Fragments/ArcItemFragment_GrantedAbilities.h"

bool FArcQuickBarInputBindHandling::OnAddedToQuickBar(UArcCoreAbilitySystemComponent* InArcASC
													  , TArray<const FArcItemData*> InSlots) const
{
	bool bHandled = true;
	for (const FArcItemData* SDE : InSlots)
	{
		const FArcItemInstance_GrantedAbilities* GrantedAbilities = ArcItems::FindInstance<FArcItemInstance_GrantedAbilities>(SDE);
		if (GrantedAbilities && GrantedAbilities->GetGrantedAbilities().Num() > 0)
		{
			for (const FGameplayAbilitySpecHandle& H : GrantedAbilities->GetGrantedAbilities())
			{
				FGameplayAbilitySpec* Spec = InArcASC->FindAbilitySpecFromHandle(H);
				if (Spec == nullptr || Spec->GetPrimaryInstance() == nullptr)
				{
					bHandled = false;
					continue;
				}
				if (Spec->GetPrimaryInstance()->GetAssetTags().HasTagExact(AbilityRequiredTag))
				{
					InArcASC->AddAbilityDynamicTag(H, TagInput);
				}
			}
		}
	}

	return bHandled;
}

bool FArcQuickBarInputBindHandling::OnRemovedFromQuickBar(UArcCoreAbilitySystemComponent* InArcASC
														  , TArray<const FArcItemData*> InSlots) const
{
	bool bHandled = true;
	for (const FArcItemData* SDE : InSlots)
	{
		const FArcItemInstance_GrantedAbilities* GrantedAbilities = ArcItems::FindInstance<FArcItemInstance_GrantedAbilities>(SDE);
		if (GrantedAbilities && GrantedAbilities->GetGrantedAbilities().Num() > 0)
		{
			for (const FGameplayAbilitySpecHandle& H : GrantedAbilities->GetGrantedAbilities())
			{
				FGameplayAbilitySpec* Spec = InArcASC->FindAbilitySpecFromHandle(H);
				if (Spec == nullptr || Spec->GetPrimaryInstance() == nullptr)
				{
					bHandled = false;
					continue;
				}
				if (Spec->GetPrimaryInstance()->GetAssetTags().HasTagExact(AbilityRequiredTag))
				{
					InArcASC->RemoveAbilityDynamicTag(H, TagInput);
				}
			}
		}
	}
	return bHandled;
}