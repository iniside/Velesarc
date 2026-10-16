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

#include "Equipment/Fragments/ArcItemFragment_ActorAttachment.h"

#include "Items/Fragments/ArcFragmentDescription.h"

#if WITH_EDITOR
FArcFragmentDescription FArcItemFragment_ActorAttachment::GetDescription(const UScriptStruct* InStruct) const
{
	FArcFragmentDescription Desc = FArcItemFragment::GetDescription(InStruct);
	Desc.CommonPairings = {
		FName(TEXT("FArcItemFragment_SocketSlots")),
		FName(TEXT("FArcItemFragment_ItemAttachmentSlots"))
	};
	Desc.UsageNotes = TEXT(
		"Spawns and attaches a full actor to the character when equipped. "
		"AttachTags must match the socket's tag requirements. "
		"Use bAttachToCharacterMesh to attach to the character's skeletal mesh instead of root. "
		"The Mass-side actor lifecycle is managed by FArcMassAttachmentHandler_Actor in ArcMassItems. "
		"For simpler visuals, prefer StaticMesh or SkeletalMesh attachment fragments.");
	return Desc;
}
#endif // WITH_EDITOR
