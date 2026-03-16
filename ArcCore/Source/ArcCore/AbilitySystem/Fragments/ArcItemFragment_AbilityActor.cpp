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



#include "ArcItemFragment_AbilityActor.h"

#if WITH_EDITOR
FArcFragmentDescription FArcItemFragment_AbilityActor::GetDescription(const UScriptStruct* InStruct) const
{
	FArcFragmentDescription Desc = FArcItemFragment::GetDescription(InStruct);
	Desc.CommonPairings = {
		FName(TEXT("FArcItemFragment_GrantedAbilities"))
	};
	Desc.UsageNotes = TEXT(
		"References a single actor class spawned by abilities for this item. "
		"Commonly used for projectiles, area effects, or summons. "
		"For items with multiple ability actors keyed by tag, use FArcItemFragment_AbilityActorMap instead.");
	return Desc;
}

FArcFragmentDescription FArcItemFragment_AbilityActorMap::GetDescription(const UScriptStruct* InStruct) const
{
	FArcFragmentDescription Desc = FArcItemFragment::GetDescription(InStruct);
	Desc.CommonPairings = {
		FName(TEXT("FArcItemFragment_GrantedAbilities"))
	};
	Desc.UsageNotes = TEXT(
		"Maps gameplay tags to ability actor classes, enabling different actor types per ability event. "
		"Use when an item has multiple ability actors keyed by action type (e.g., primary fire vs alt fire). "
		"For a single actor class, use FArcItemFragment_AbilityActor instead.");
	return Desc;
}
#endif // WITH_EDITOR
