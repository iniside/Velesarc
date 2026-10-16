// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassAttachmentProcessor.h"

#include "ArcMassAttachmentFragment.h"
#include "Mass/EntityFragments.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"

UArcMassAttachmentProcessor::UArcMassAttachmentProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = false;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllNetModes);
}

void UArcMassAttachmentProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcMassAttachmentFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	// Only process followers that haven't already lost their target.
	EntityQuery.AddTagRequirement<FArcMassAttachmentLostTag>(EMassFragmentPresence::None);
}

void UArcMassAttachmentProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcMassAttachment);

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager](FMassExecutionContext& Ctx)
		{
			const TConstArrayView<FArcMassAttachmentFragment> AttachmentFragments = Ctx.GetFragmentView<FArcMassAttachmentFragment>();
			TArrayView<FTransformFragment> Transforms = Ctx.GetMutableFragmentView<FTransformFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FArcMassAttachmentFragment& Attachment = AttachmentFragments[EntityIt];

				if (!EntityManager.IsEntityValid(Attachment.Target))
				{
					const FMassEntityHandle FollowerEntity = Ctx.GetEntity(EntityIt);
					Ctx.Defer().AddTag<FArcMassAttachmentLostTag>(FollowerEntity);
					continue;
				}

				const FTransformFragment& TargetTransform = EntityManager.GetFragmentDataChecked<FTransformFragment>(Attachment.Target);
				FTransformFragment& FollowerTransform = Transforms[EntityIt];

				FollowerTransform.SetTransform(Attachment.LocalOffset * TargetTransform.GetTransform());
			}
		});
}
