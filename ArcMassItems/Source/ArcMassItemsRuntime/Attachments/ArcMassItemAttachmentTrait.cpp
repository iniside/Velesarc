// Copyright Lukasz Baran. All Rights Reserved.

#include "Attachments/ArcMassItemAttachmentTrait.h"
#include "Attachments/ArcMassItemAttachmentFragments.h"
#include "Attachments/ArcMassItemAttachmentConfig.h"
#include "Attachments/ArcMassAttachmentHandler.h"
#include "Attachments/ArcMassItemAttachmentSlots.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "MassEntityTemplateRegistry.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcMassItemAttachmentTrait, Log, All);

void UArcMassItemAttachmentTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	if (!AttachmentConfig)
	{
		UE_LOG(LogArcMassItemAttachmentTrait, Warning,
			TEXT("UArcMassItemAttachmentTrait used without AttachmentConfig in '%s' - skipping"),
			*World.GetName());
		return;
	}

	// Initialize handlers idempotently before sharing the config.
	const UScriptStruct* StoreType = FArcMassItemStoreFragment::StaticStruct();
	for (FArcMassItemAttachmentSlot& Slot : const_cast<TArray<FArcMassItemAttachmentSlot>&>(AttachmentConfig->Slots))
	{
		for (FInstancedStruct& HandlerInst : Slot.Handlers)
		{
			if (FArcMassAttachmentHandler* Handler = HandlerInst.GetMutablePtr<FArcMassAttachmentHandler>())
			{
				Handler->Initialize(StoreType);
			}
		}
	}

	FArcMassItemAttachmentConfigFragment ConfigFragment;
	ConfigFragment.Config = AttachmentConfig;
	BuildContext.AddConstSharedFragment(FConstSharedStruct::Make(ConfigFragment));

	BuildContext.AddFragment<FArcMassItemAttachmentStateFragment>();
}
