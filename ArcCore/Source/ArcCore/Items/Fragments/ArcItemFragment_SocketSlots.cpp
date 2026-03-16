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

#include "ArcItemFragment_SocketSlots.h"

#if WITH_EDITOR
FArcFragmentDescription FArcItemFragment_SocketSlots::GetDescription(const UScriptStruct* InStruct) const
{
	FArcFragmentDescription Desc = FArcItemFragment::GetDescription(InStruct);
	Desc.CommonPairings = {
		FName(TEXT("FArcItemFragment_ActorAttachment")),
		FName(TEXT("FArcItemFragment_StaticMeshAttachment")),
		FName(TEXT("FArcItemFragment_SkeletalMeshAttachment")),
		FName(TEXT("FArcItemFragment_ItemAttachmentSlots"))
	};
	Desc.UsageNotes = TEXT(
		"Defines logical socket slots where child items can be attached. "
		"Each FArcSocketSlot specifies a SlotId tag, optional item type/tag filters, "
		"and an optional DefaultSocketItemDefinition for auto-attach. "
		"Can use a shared UArcItemSocketSlotsPreset data asset via the Preset property. "
		"Child items with attachment fragments match sockets via AttachTags.");
	return Desc;
}
#endif // WITH_EDITOR
