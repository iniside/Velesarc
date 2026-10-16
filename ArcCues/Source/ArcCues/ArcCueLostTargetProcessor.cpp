// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCueLostTargetProcessor.h"

#include "Engine/World.h"
#include "Mass/CommandBuffer.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"

#include "ArcCueEntityFragment.h"
#include "ArcCueSubsystem.h"
#include "ArcCuesModule.h"
#include "ArcMass/Attachment/ArcMassAttachmentFragment.h"

namespace ArcCueLostTargetProcessor
{
	struct FLostAttachment
	{
		FArcCueHandle Cue;
		int32 BehaviorIndex = INDEX_NONE;
		FMassEntityHandle Entity;
	};
}

UArcCueLostTargetProcessor::UArcCueLostTargetProcessor()
{
	// Touches UArcCueSubsystem on the game thread.
	bRequiresGameThreadExecution = true;
}

void UArcCueLostTargetProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcCueEntityFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FArcMassAttachmentLostTag>(EMassFragmentPresence::All);
}

void UArcCueLostTargetProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = GetWorld();
	UArcCueSubsystem* Subsystem = UArcCueSubsystem::Get(World);
	if (Subsystem == nullptr)
	{
		return;
	}

	TArray<ArcCueLostTargetProcessor::FLostAttachment, TInlineAllocator<16>> Orphaned;

	EntityQuery.ForEachEntityChunk(Context, [&Orphaned](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FArcCueEntityFragment> CueFragments = ChunkContext.GetFragmentView<FArcCueEntityFragment>();
		const int32 NumEntities = ChunkContext.GetNumEntities();

		for (int32 Index = 0; Index < NumEntities; ++Index)
		{
			ArcCueLostTargetProcessor::FLostAttachment& Lost = Orphaned.AddDefaulted_GetRef();
			Lost.Cue = CueFragments[Index].OwningCue;
			Lost.BehaviorIndex = CueFragments[Index].BehaviorIndex;
			Lost.Entity = ChunkContext.GetEntity(Index);
		}
	});

	for (const ArcCueLostTargetProcessor::FLostAttachment& Lost : Orphaned)
	{
		if (Lost.Cue.IsValid())
		{
			Subsystem->OnCueAttachmentLost(Lost.Cue, Lost.BehaviorIndex, Lost.Entity);
		}
		Context.Defer().DestroyEntity(Lost.Entity);
	}
}
