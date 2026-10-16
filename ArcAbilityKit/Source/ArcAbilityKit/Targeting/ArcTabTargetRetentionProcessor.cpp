// Copyright Lukasz Baran. All Rights Reserved.

#include "Targeting/ArcTabTargetRetentionProcessor.h"

#include "ArcTargetingPipelineTypes.h"
#include "ArcTargetingSubsystem.h"
#include "Mass/EntityFragments.h"
#include "Mass/ExecutionContext.h"
#include "Targeting/ArcTabTargetingFragment.h"
#include "Targeting/ArcTabTargetingState.h"

UArcTabTargetRetentionProcessor::UArcTabTargetRetentionProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
	QueryBasedPruning = EMassQueryBasedPruning::Never;
}

void UArcTabTargetRetentionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddSparseRequirement<FArcTabTargetingFragment>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
}

void UArcTabTargetRetentionProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = Context.GetWorld();
	UArcTargetingSubsystem* Subsystem = UArcTargetingSubsystem::Get(World);
	if (World == nullptr || Subsystem == nullptr)
	{
		return;
	}

	EntityQuery.ForEachEntityChunk(Context, [&EntityManager, World, Subsystem](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FTransformFragment> SourceTransforms = ChunkContext.GetFragmentView<FTransformFragment>();
		for (FMassExecutionContext::FSparseEntityIterator EntityIt = ChunkContext.CreateSparseEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcTabTargetingFragment* State = ChunkContext.GetMutableSparseElement<FArcTabTargetingFragment>(EntityIt);
			if (State == nullptr || !State->SelectedTargetTag.IsValid())
			{
				continue;
			}

			const FMassEntityHandle SourceEntity = ChunkContext.GetEntity(EntityIt);
			const FArcTargetCandidate* SelectedCandidate = ArcTabTargetingState::GetSelectedCandidate(*Subsystem, SourceEntity, State->SelectedTargetTag);
			if (SelectedCandidate == nullptr)
			{
				continue;
			}

			const FArcTargetCandidate Selected = *SelectedCandidate;
			FVector TargetLocation = FVector::ZeroVector;
			EArcTabTargetChangeReason FailureReason = EArcTabTargetChangeReason::InvalidTarget;
			bool bSelectable = false;
			if (!SourceTransforms.IsEmpty())
			{
				bSelectable = ArcTabTargetingState::IsSelectable(
					EntityManager,
					SourceEntity,
					SourceTransforms[EntityIt].GetTransform().GetLocation(),
					Selected.Entity,
					State->RetentionPolicy,
					TargetLocation,
					FailureReason);
			}

			if (bSelectable)
			{
				ArcTabTargetingState::PublishSelectedTarget(*Subsystem, SourceEntity, State->SelectedTargetTag, Selected, TargetLocation);
				continue;
			}

			Subsystem->ClearPublishedResult(SourceEntity, State->SelectedTargetTag);
			ArcTabTargetingState::ResetCycle(*State);
			UE_LOGF(LogArcTabTargeting, Log, "Entity %ls lost target %ls under %ls: %ls.",
				*SourceEntity.DebugGetDescription(),
				*Selected.Entity.DebugGetDescription(),
				*State->SelectedTargetTag.ToString(),
				ArcTabTargetingState::LexReason(FailureReason));
			ArcTabTargetingState::BroadcastTargetChanged(*World, SourceEntity, Selected.Entity, FMassEntityHandle(), FailureReason);
		}
	});
}
