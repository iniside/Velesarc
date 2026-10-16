// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/PlacedEntities/ArcPlacedEntityOwnerObserver.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityRespawnTrait.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityPartitionActor.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcPlacedEntityOwnerObserver)

UArcPlacedEntityOwnerObserver::UArcPlacedEntityOwnerObserver()
    : ObserverQuery{*this}
{
	ObservedTypes.Add(FArcPlacedEntityOwnerFragment::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcPlacedEntityOwnerObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcPlacedEntityOwnerFragment>(EMassFragmentAccess::ReadOnly);
}

void UArcPlacedEntityOwnerObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	ObserverQuery.ForEachEntityChunk(Context,
		[](FMassExecutionContext& Ctx)
		{
			TConstArrayView<FArcPlacedEntityOwnerFragment> OwnerFragments =
				Ctx.GetFragmentView<FArcPlacedEntityOwnerFragment>();

			for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
			{
				const FArcPlacedEntityOwnerFragment& OwnerFrag = OwnerFragments[i];
				AArcPlacedEntityPartitionActor* Actor = OwnerFrag.OwnerPartitionActor.Get();
				if (Actor != nullptr)
				{
					Actor->RecordRemovalForRespawn(OwnerFrag.EntryIndex, OwnerFrag.TransformIndex);
				}
			}
		});
}
