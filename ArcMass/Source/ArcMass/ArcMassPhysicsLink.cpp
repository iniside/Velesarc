// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassPhysicsLink.h"
#include "ArcMassPhysicsEntityLink.h"
#include "Chaos/ChaosUserEntity.h"
#include "MassEntityTemplateRegistry.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassPhysicsLink)

// ---------------------------------------------------------------------------
// Signal Processor
// ---------------------------------------------------------------------------

UArcMassPhysicsLinkProcessor::UArcMassPhysicsLinkProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UArcMassPhysicsLinkProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::PhysicsLinkRequested);
}

void UArcMassPhysicsLinkProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcMassPhysicsLinkFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FArcMassPhysicsLinkTag>(EMassFragmentPresence::All);
}

void UArcMassPhysicsLinkProcessor::SignalEntities(FMassEntityManager& EntityManager,
	FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	EntityQuery.ForEachEntityChunk(Context,
		[](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcMassPhysicsLinkFragment> LinkFragments =
				Ctx.GetMutableFragmentView<FArcMassPhysicsLinkFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcMassPhysicsLinkFragment& Fragment = LinkFragments[EntityIt];
				FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				for (FArcMassPhysicsLinkEntry& Entry : Fragment.Entries)
				{
					if (Entry.Append != nullptr)
					{
						continue;
					}

					UPrimitiveComponent* Comp = Entry.Component.Get();
					if (!Comp)
					{
						continue;
					}

					FBodyInstance* Body = nullptr;
					if (Entry.BodyIndex != INDEX_NONE)
					{
						Body = Comp->GetBodyInstance(NAME_None, false, Entry.BodyIndex);
					}
					else
					{
						Body = Comp->GetBodyInstance();
					}

					if (!Body)
					{
						continue;
					}

					Entry.Append = ArcMassPhysicsEntityLink::Attach(*Body, Entity, Comp);
				}
			}
		});
}

// ---------------------------------------------------------------------------
// Deinit Observer
// ---------------------------------------------------------------------------

UArcMassPhysicsLinkDeinitObserver::UArcMassPhysicsLinkDeinitObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcMassPhysicsLinkTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcMassPhysicsLinkDeinitObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcMassPhysicsLinkFragment>(EMassFragmentAccess::ReadWrite);
}

void UArcMassPhysicsLinkDeinitObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	ObserverQuery.ForEachEntityChunk(Context,
		[](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcMassPhysicsLinkFragment> LinkFragments =
				Ctx.GetMutableFragmentView<FArcMassPhysicsLinkFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcMassPhysicsLinkFragment& Fragment = LinkFragments[EntityIt];

				for (FArcMassPhysicsLinkEntry& Entry : Fragment.Entries)
				{
					if (!Entry.Append)
					{
						continue;
					}

					UPrimitiveComponent* Comp = Entry.Component.Get();
					FBodyInstance* Body = nullptr;

					if (Comp)
					{
						if (Entry.BodyIndex != INDEX_NONE)
						{
							Body = Comp->GetBodyInstance(NAME_None, false, Entry.BodyIndex);
						}
						else
						{
							Body = Comp->GetBodyInstance();
						}
					}

					if (Body)
					{
						ArcMassPhysicsEntityLink::Detach(*Body, Entry.Append);
					}
					else
					{
						delete Entry.Append->UserDefinedEntity;
						Entry.Append->UserDefinedEntity = nullptr;
						delete Entry.Append;
					}
					Entry.Append = nullptr;
				}

				Fragment.Entries.Empty();
			}
		});
}

// ---------------------------------------------------------------------------
// Trait
// ---------------------------------------------------------------------------

void UArcMassPhysicsLinkTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FArcMassPhysicsLinkFragment>();
	BuildContext.AddTag<FArcMassPhysicsLinkTag>();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void ArcMassPhysicsLink::PopulateEntriesFromActor(FArcMassPhysicsLinkFragment& LinkFragment, AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	TInlineComponentArray<UPrimitiveComponent*, 8> PrimitiveComponents;
	Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (UPrimitiveComponent* PrimComp : PrimitiveComponents)
	{
		if (!PrimComp || !PrimComp->IsPhysicsStateCreated())
		{
			continue;
		}

		FBodyInstance* Body = PrimComp->GetBodyInstance();
		if (!Body)
		{
			continue;
		}

		FArcMassPhysicsLinkEntry Entry;
		Entry.Component = PrimComp;
		Entry.BodyIndex = INDEX_NONE;
		LinkFragment.Entries.Add(Entry);
	}
}
