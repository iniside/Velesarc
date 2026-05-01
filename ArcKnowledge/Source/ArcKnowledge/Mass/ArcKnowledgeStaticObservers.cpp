// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcKnowledgeStaticObservers.h"
#include "Mass/ArcKnowledgeStaticFragment.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeRTree.h"
#include "ArcKnowledgeTagBitmask.h"
#include "Mass/EntityFragments.h"
#include "MassExecutionContext.h"

// ====================================================================
// Add Observer
// ====================================================================

UArcKnowledgeStaticAddObserver::UArcKnowledgeStaticAddObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcKnowledgeStaticTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
}

void UArcKnowledgeStaticAddObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddConstSharedRequirement<FArcKnowledgeStaticConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddTagRequirement<FArcKnowledgeStaticTag>(EMassFragmentPresence::All);
}

void UArcKnowledgeStaticAddObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UArcKnowledgeSubsystem* Subsystem = Context.GetWorld()->GetSubsystem<UArcKnowledgeSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcKnowledgeStaticAdd);

	FArcKnowledgeRTree& RTree = Subsystem->GetStaticRTree();
	FArcKnowledgeTagBitmaskRegistry& Registry = Subsystem->GetTagRegistry();
	const double WorldTime = Context.GetWorld()->GetTimeSeconds();

	ObserverQuery.ForEachEntityChunk(Context, [Subsystem, &RTree, &Registry, WorldTime](FMassExecutionContext& Ctx)
	{
		TConstArrayView<FTransformFragment> TransformFragments = Ctx.GetFragmentView<FTransformFragment>();
		const FArcKnowledgeStaticConfigFragment& Config = Ctx.GetConstSharedFragment<FArcKnowledgeStaticConfigFragment>();

		const FArcKnowledgeTagBitmask Bitmask = Registry.BuildBitmaskWithRegistration(Config.KnowledgeTags);

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FVector Location = TransformFragments[EntityIt].GetTransform().GetLocation();
			const FMassEntityHandle EntityHandle = Ctx.GetEntity(EntityIt);

			// Build full entry from config
			FArcKnowledgeEntry Entry;

			if (Config.Definition)
			{
				const UArcKnowledgeEntryDefinition* Definition = Config.Definition;
				Entry.Tags = Definition->Tags;
				Entry.Payload = Definition->Payload;
				Entry.SpatialBroadcastRadius = Definition->SpatialBroadcastRadius;
			}
			else
			{
				Entry.Tags = Config.KnowledgeTags;
			}

			Entry.Location = Location;
			Entry.SourceEntity = EntityHandle;
			Entry.bPersistent = true;
			Entry.Relevance = 1.0f;
			Entry.Timestamp = WorldTime;

			// Register in subsystem — lands in TMap + all indices
			const FArcKnowledgeHandle Handle = Subsystem->RegisterKnowledge(Entry);

			// Insert into R-tree for spatial acceleration
			FArcKnowledgeRTreeLeafEntry LeafEntry;
			LeafEntry.Entity = EntityHandle;
			LeafEntry.Position = Location;
			LeafEntry.TagBitmask = Bitmask;
			LeafEntry.Handle = Handle;

			RTree.Insert(LeafEntry);
		}
	});
}

// ====================================================================
// Remove Observer
// ====================================================================

UArcKnowledgeStaticRemoveObserver::UArcKnowledgeStaticRemoveObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcKnowledgeStaticTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcKnowledgeStaticRemoveObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddTagRequirement<FArcKnowledgeStaticTag>(EMassFragmentPresence::All);
}

void UArcKnowledgeStaticRemoveObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UArcKnowledgeSubsystem* Subsystem = Context.GetWorld()->GetSubsystem<UArcKnowledgeSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcKnowledgeStaticRemove);

	FArcKnowledgeRTree& RTree = Subsystem->GetStaticRTree();

	ObserverQuery.ForEachEntityChunk(Context, [Subsystem, &RTree](FMassExecutionContext& Ctx)
	{
		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FMassEntityHandle EntityHandle = Ctx.GetEntity(EntityIt);

			// Clean up all knowledge entries owned by this entity
			Subsystem->RemoveKnowledgeBySource(EntityHandle);

			// Remove from R-tree spatial acceleration
			RTree.Remove(EntityHandle);
		}
	});
}
