// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcKnowledgeObservers.h"
#include "Mass/ArcKnowledgeFragments.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeEntry.h"
#include "MassEntityFragments.h"
#include "MassEntityView.h"
#include "MassExecutionContext.h"

// ====================================================================
// Add Observer
// ====================================================================

UArcKnowledgeMemberAddObserver::UArcKnowledgeMemberAddObserver()
	: ObserverQuery{*this}
{
	ObservedType = FArcKnowledgeMemberTag::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
}

void UArcKnowledgeMemberAddObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcKnowledgeMemberFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddConstSharedRequirement<FArcKnowledgeMemberConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddTagRequirement<FArcKnowledgeMemberTag>(EMassFragmentPresence::All);
	ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
}

void UArcKnowledgeMemberAddObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UArcKnowledgeSubsystem* Subsystem = Context.GetWorld()->GetSubsystem<UArcKnowledgeSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	ObserverQuery.ForEachEntityChunk(Context, [Subsystem, &EntityManager](FMassExecutionContext& Ctx)
	{
		TArrayView<FArcKnowledgeMemberFragment> MemberFragments = Ctx.GetMutableFragmentView<FArcKnowledgeMemberFragment>();
		const FArcKnowledgeMemberConfigFragment& Config = Ctx.GetConstSharedFragment<FArcKnowledgeMemberConfigFragment>();
		TConstArrayView<FTransformFragment> TransformFragments = Ctx.GetFragmentView<FTransformFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcKnowledgeMemberFragment& Member = MemberFragments[EntityIt];
			const FVector Location = TransformFragments[EntityIt].GetTransform().GetLocation();
			const FMassEntityHandle EntityHandle = Ctx.GetEntity(EntityIt);

			// Register entries from the definition if set (primary registration)
			if (Config.Definition)
			{
				Subsystem->RegisterFromDefinition(Config.Definition, Location, EntityHandle, Member.RegisteredKnowledgeHandles);
			}

			// Register each capability tag as a knowledge entry
			for (const FGameplayTag& Tag : Config.CapabilityTags)
			{
				FArcKnowledgeEntry Entry;
				Entry.Tags.AddTag(Tag);
				Entry.Location = Location;
				Entry.SourceEntity = EntityHandle;
				Entry.Relevance = 1.0f;

				const FArcKnowledgeHandle KnowledgeHandle = Subsystem->RegisterKnowledge(Entry);
				Member.RegisteredKnowledgeHandles.Add(KnowledgeHandle);
			}

			// Register the role if set
			if (Member.Role.IsValid())
			{
				FArcKnowledgeEntry RoleEntry;
				RoleEntry.Tags.AddTag(Member.Role);
				RoleEntry.Location = Location;
				RoleEntry.SourceEntity = EntityHandle;
				RoleEntry.Relevance = 1.0f;

				const FArcKnowledgeHandle KnowledgeHandle = Subsystem->RegisterKnowledge(RoleEntry);
				Member.RegisteredKnowledgeHandles.Add(KnowledgeHandle);
			}
		}
	});
}

// ====================================================================
// Remove Observer
// ====================================================================

UArcKnowledgeMemberRemoveObserver::UArcKnowledgeMemberRemoveObserver()
	: ObserverQuery{*this}
{
	ObservedType = FArcKnowledgeMemberTag::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcKnowledgeMemberRemoveObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcKnowledgeMemberFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddTagRequirement<FArcKnowledgeMemberTag>(EMassFragmentPresence::All);
}

void UArcKnowledgeMemberRemoveObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UArcKnowledgeSubsystem* Subsystem = Context.GetWorld()->GetSubsystem<UArcKnowledgeSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	ObserverQuery.ForEachEntityChunk(Context, [Subsystem](FMassExecutionContext& Ctx)
	{
		TArrayView<FArcKnowledgeMemberFragment> MemberFragments = Ctx.GetMutableFragmentView<FArcKnowledgeMemberFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcKnowledgeMemberFragment& Member = MemberFragments[EntityIt];

			// Remove all knowledge entries registered by this entity
			for (const FArcKnowledgeHandle& Handle : Member.RegisteredKnowledgeHandles)
			{
				Subsystem->RemoveKnowledge(Handle);
			}
			Member.RegisteredKnowledgeHandles.Empty();
		}
	});
}
