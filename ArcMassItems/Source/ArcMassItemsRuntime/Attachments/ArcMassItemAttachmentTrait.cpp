// Copyright Lukasz Baran. All Rights Reserved.

#include "Attachments/ArcMassItemAttachmentTrait.h"
#include "Attachments/ArcMassItemAttachmentFragments.h"
#include "Attachments/ArcMassItemAttachmentConfig.h"
#include "Attachments/ArcMassAttachmentHandler.h"
#include "Attachments/ArcMassItemAttachmentSlots.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Mass/CommandBuffer.h"
#include "MassEntityTemplateRegistry.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcMassItemAttachmentTrait, Log, All);

void UArcMassItemAttachmentTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	if (!AttachmentConfig)
	{
		UE_LOGF(LogArcMassItemAttachmentTrait, Warning,"UArcMassItemAttachmentTrait used without AttachmentConfig in '%ls' - skipping",
			*World.GetName());
		return;
	}

	// Resolve the set of store types this trait subscribes to. Default = base store.
	TArray<TObjectPtr<UScriptStruct>> ResolvedStoreTypes = InterestedStoreTypes;
	if (ResolvedStoreTypes.IsEmpty())
	{
		ResolvedStoreTypes.Add(FArcMassItemStoreFragment::StaticStruct());
	}

	// Initialize handlers idempotently for every interested store type.
	for (FArcMassItemAttachmentSlot& Slot : const_cast<TArray<FArcMassItemAttachmentSlot>&>(AttachmentConfig->Slots))
	{
		for (FInstancedStruct& HandlerInst : Slot.Handlers)
		{
			if (FArcMassAttachmentHandler* Handler = HandlerInst.GetMutablePtr<FArcMassAttachmentHandler>())
			{
				for (const TObjectPtr<UScriptStruct>& StoreType : ResolvedStoreTypes)
				{
					Handler->Initialize(StoreType);
				}
			}
		}
	}

	FArcMassItemAttachmentConfigFragment ConfigFragment;
	ConfigFragment.Config = AttachmentConfig;
	ConfigFragment.InterestedStoreTypes = MoveTemp(ResolvedStoreTypes);
	BuildContext.AddConstSharedFragment(FConstSharedStruct::Make(ConfigFragment));

	BuildContext.AddTag<FArcMassItemAttachmentTag>();
}

void UArcMassItemAttachmentTrait::BuildSparseFragments(FArcSparseFragmentBuildContext& Out, const UWorld& World) const
{
	Out.AddSparseFragment<FArcMassItemAttachmentStateFragment>();
}
