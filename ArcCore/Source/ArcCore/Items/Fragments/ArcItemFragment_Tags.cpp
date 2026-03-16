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

#include "ArcItemFragment_Tags.h"

#if WITH_EDITOR
FArcFragmentDescription FArcItemFragment_Tags::GetDescription(const UScriptStruct* InStruct) const
{
	FArcFragmentDescription Desc = FArcItemFragment::GetDescription(InStruct);
	Desc.UsageNotes = TEXT(
		"Foundational fragment for item categorization. "
		"ItemTags identify the item type (indexed by asset registry). "
		"AssetTags are additional indexed tags for search/filter (e.g., item family for random spawning). "
		"GrantedTags are applied to the owning actor when equipped. "
		"RequiredTags and DenyTags gate whether the item can be equipped or seen. "
		"Most items should have this fragment.");
	return Desc;
}
#endif // WITH_EDITOR
