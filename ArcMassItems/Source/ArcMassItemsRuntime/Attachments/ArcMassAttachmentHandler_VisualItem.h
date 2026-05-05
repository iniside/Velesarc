// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Attachments/ArcMassAttachmentHandler.h"
#include "Equipment/Fragments/ArcItemFragment_ItemVisualAttachment.h"
#include "ArcMassAttachmentHandler_VisualItem.generated.h"

USTRUCT(meta = (DisplayName = "Mass Visual Item Attachment"))
struct ARCMASSITEMSRUNTIME_API FArcMassAttachmentHandler_VisualItem : public FArcMassAttachmentHandlerCommon
{
	GENERATED_BODY()

public:
	virtual bool HandleItemAddedToSlot(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FArcMassItemAttachmentStateFragment& State,
		const FArcItemData* Item,
		const FArcItemData* OwnerItem) const override;

	virtual void HandleItemAttach(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FArcMassItemAttachmentStateFragment& State,
		const FArcItemId ItemId,
		const FArcItemId OwnerItem) const override;

	virtual void HandleItemRemovedFromSlot(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FArcMassItemAttachmentStateFragment& State,
		const FArcMassItemAttachment& Snapshot) const override {}

	virtual void HandleItemDetach(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FArcMassItemAttachmentStateFragment& State,
		const FArcMassItemAttachment& Snapshot) const override;

	virtual UScriptStruct* SupportedItemFragment() const override
	{
		return FArcItemFragment_ItemVisualAttachment::StaticStruct();
	}
};
