// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSettlementObservers.h"
#include "ArcSettlementFragments.h"
#include "ArcSettlementSubsystem.h"
#include "ArcKnowledgeEntry.h"
#include "MassEntityFragments.h"
#include "MassEntityView.h"
#include "MassExecutionContext.h"

// ====================================================================
// Add Observer
// ====================================================================

UArcSettlementMemberAddObserver::UArcSettlementMemberAddObserver()
	: ObserverQuery{*this}
{
	ObservedType = FArcSettlementMemberTag::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
}

void UArcSettlementMemberAddObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcSettlementMemberFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddConstSharedRequirement<FArcSettlementMemberConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddTagRequirement<FArcSettlementMemberTag>(EMassFragmentPresence::All);
	ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
}

void UArcSettlementMemberAddObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UArcSettlementSubsystem* Subsystem = Context.GetWorld()->GetSubsystem<UArcSettlementSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	ObserverQuery.ForEachEntityChunk(Context, [Subsystem, &EntityManager](FMassExecutionContext& Ctx)
	{
		TArrayView<FArcSettlementMemberFragment> MemberFragments = Ctx.GetMutableFragmentView<FArcSettlementMemberFragment>();
		const FArcSettlementMemberConfigFragment& Config = Ctx.GetConstSharedFragment<FArcSettlementMemberConfigFragment>();
		TConstArrayView<FTransformFragment> TransformFragments = Ctx.GetFragmentView<FTransformFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcSettlementMemberFragment& Member = MemberFragments[EntityIt];
			const FVector Location = TransformFragments[EntityIt].GetTransform().GetLocation();

			// If no settlement assigned, try to find one at the entity's location
			if (!Member.SettlementHandle.IsValid())
			{
				Member.SettlementHandle = Subsystem->FindSettlementAt(Location);
			}

			if (!Member.SettlementHandle.IsValid())
			{
				continue;
			}

			const FMassEntityHandle EntityHandle = Ctx.GetEntity(EntityIt);

			// Register each capability tag as a knowledge entry
			for (const FGameplayTag& Tag : Config.CapabilityTags)
			{
				FArcKnowledgeEntry Entry;
				Entry.Tags.AddTag(Tag);
				Entry.Location = Location;
				Entry.Settlement = Member.SettlementHandle;
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
				RoleEntry.Settlement = Member.SettlementHandle;
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

UArcSettlementMemberRemoveObserver::UArcSettlementMemberRemoveObserver()
	: ObserverQuery{*this}
{
	ObservedType = FArcSettlementMemberTag::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcSettlementMemberRemoveObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcSettlementMemberFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddTagRequirement<FArcSettlementMemberTag>(EMassFragmentPresence::All);
}

void UArcSettlementMemberRemoveObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UArcSettlementSubsystem* Subsystem = Context.GetWorld()->GetSubsystem<UArcSettlementSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	ObserverQuery.ForEachEntityChunk(Context, [Subsystem](FMassExecutionContext& Ctx)
	{
		TArrayView<FArcSettlementMemberFragment> MemberFragments = Ctx.GetMutableFragmentView<FArcSettlementMemberFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcSettlementMemberFragment& Member = MemberFragments[EntityIt];

			// Remove all knowledge entries registered by this entity
			for (const FArcKnowledgeHandle& Handle : Member.RegisteredKnowledgeHandles)
			{
				Subsystem->RemoveKnowledge(Handle);
			}
			Member.RegisteredKnowledgeHandles.Empty();
		}
	});
}
