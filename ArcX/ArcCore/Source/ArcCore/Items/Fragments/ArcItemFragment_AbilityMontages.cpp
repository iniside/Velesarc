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



#include "ArcItemFragment_AbilityMontages.h"

#if WITH_EDITOR
FArcFragmentDescription FArcItemFragment_AbilityMontages::GetDescription(const UScriptStruct* InStruct) const
{
	FArcFragmentDescription Desc = FArcItemFragment::GetDescription(InStruct);
	Desc.CommonPairings = {
		FName(TEXT("FArcItemFragment_GrantedAbilities")),
		FName(TEXT("FArcItemFragment_AbilityEffectsToApply"))
	};
	Desc.UsageNotes = TEXT(
		"Defines animation montages used by gameplay abilities on this item. "
		"StartMontage plays on ability activation. EventTagToMontageMap maps "
		"gameplay event tags to specific montages for combo or branching animations. "
		"Abilities look up montages from this fragment at runtime.");
	return Desc;
}
#endif // WITH_EDITOR
