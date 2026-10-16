// Copyright Lukasz Baran. All Rights Reserved.

#include "Attachments/ArcMassItemAttachmentObserver.h"

#include "Attachments/ArcMassItemAttachmentTrait.h"
#include "Attachments/ArcMassItemAttachmentSubsystem.h"
#include "Events/ArcMassItemEventSubsystem.h"

#include "Mass/EntityManager.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"

UArcMassItemAttachmentTagRemoveObserver::UArcMassItemAttachmentTagRemoveObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcMassItemAttachmentTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcMassItemAttachmentTagRemoveObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManagerRef)
{
	// Minimal query — we process per-entity via subsystem calls, not via chunk fragments.
	// Tag is already guaranteed present by observer registration.
	ObserverQuery.AddTagRequirement<FArcMassItemAttachmentTag>(EMassFragmentPresence::All);
}

void UArcMassItemAttachmentTagRemoveObserver::Execute(
	FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World) return;

	UArcMassItemAttachmentSubsystem* AttachmentSub =
		World->GetSubsystem<UArcMassItemAttachmentSubsystem>();
	UArcMassItemEventSubsystem* EventSub =
		World->GetSubsystem<UArcMassItemEventSubsystem>();

	if (!AttachmentSub || !EventSub) return;

	ObserverQuery.ForEachEntityChunk(Context,
		[AttachmentSub, EventSub](FMassExecutionContext& Ctx)
		{
			for (FMassExecutionContext::FEntityIterator It = Ctx.CreateEntityIterator(); It; ++It)
			{
				FMassEntityHandle Entity = Ctx.GetEntity(It);
				AttachmentSub->TeardownEntity(Entity);
				EventSub->EvictEntity(Entity);
			}
		});
}
