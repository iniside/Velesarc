// Copyright Lukasz Baran. All Rights Reserved.

#include "Processors/ArcMassEntityReplicationObserver.h"
#include "Fragments/ArcMassReplicatedTag.h"
#include "Fragments/ArcMassReplicationConfigFragment.h"
#include "Fragments/ArcMassNetIdFragment.h"
#include "Subsystem/ArcMassEntityReplicationSubsystem.h"
#include "Replication/ArcMassEntityReplicationProxy.h"
#include "Replication/ArcMassReplicationDescriptorSet.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "MassEntityView.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityElementTypes.h"
#include "StructUtils/InstancedStruct.h"

// --- Start Observer ---

UArcMassEntityReplicationStartObserver::UArcMassEntityReplicationStartObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcMassEntityReplicatedTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
}

void UArcMassEntityReplicationStartObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcMassEntityNetIdFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddConstSharedRequirement<FArcMassEntityReplicationConfigFragment>();
	ObserverQuery.AddTagRequirement<FArcMassEntityReplicatedTag>(EMassFragmentPresence::All);
	ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	ObserverQuery.RegisterWithProcessor(*this);
}

void UArcMassEntityReplicationStartObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	UArcMassEntityReplicationSubsystem* Subsystem = World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	ObserverQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem](FMassExecutionContext& Context)
		{
			const FArcMassEntityReplicationConfigFragment& Config =
				Context.GetConstSharedFragment<FArcMassEntityReplicationConfigFragment>();

			TArray<const UScriptStruct*> FragmentTypes;
			for (const FArcMassReplicatedFragmentEntry& Entry : Config.ReplicatedFragmentEntries)
			{
				if (Entry.FragmentType)
				{
					FragmentTypes.Add(Entry.FragmentType);
				}
			}

			// Build sorted archetype key (same sort as GetOrCreateProxy)
			UArcMassEntityReplicationSubsystem::FArchetypeKey ArchetypeKey;
			ArchetypeKey.SortedTypes = FragmentTypes;
			Algo::Sort(ArchetypeKey.SortedTypes, [](const UScriptStruct* A, const UScriptStruct* B) { return A->GetFName().LexicalLess(B->GetFName()); });

			AArcMassEntityReplicationProxy* Proxy = Subsystem->GetOrCreateProxy(FragmentTypes, Config.CullDistance, Config.EntityConfigAsset);
			if (!Proxy)
			{
				return;
			}

			Subsystem->GetOrCreateGrid(ArchetypeKey, Config.CellSize);

			const ArcMassReplication::FArcMassReplicationDescriptorSet& DescSet = Proxy->GetDescriptorSet();

			const TArrayView<FArcMassEntityNetIdFragment> NetIdFragments =
				Context.GetMutableFragmentView<FArcMassEntityNetIdFragment>();

			const bool bHasTransform = Context.GetFragmentView<FTransformFragment>().Num() > 0;
			const TConstArrayView<FTransformFragment> Transforms =
				bHasTransform ? Context.GetFragmentView<FTransformFragment>() : TConstArrayView<FTransformFragment>();

			for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); ++EntityIndex)
			{
				FMassEntityHandle Entity = Context.GetEntity(EntityIndex);
				FArcMassEntityNetIdFragment& NetIdFragment = NetIdFragments[EntityIndex];

				FArcMassNetId NetId = Subsystem->AllocateNetId();
				NetIdFragment.NetId = NetId;
				Subsystem->RegisterEntityNetId(NetId, Entity);

				TArray<FInstancedStruct> FragmentSlots;
				FragmentSlots.SetNum(DescSet.FragmentTypes.Num());
				FMassEntityView EntityView(EntityManager, Entity);
				for (int32 SlotIdx = 0; SlotIdx < DescSet.FragmentTypes.Num(); ++SlotIdx)
				{
					const UScriptStruct* StructType = DescSet.FragmentTypes[SlotIdx];
					FragmentSlots[SlotIdx].InitializeAs(StructType);

					const uint8* SrcMemory = nullptr;
					if (UE::Mass::IsSparse(StructType))
					{
						FConstStructView SparseView = EntityManager.GetSparseElementDataForEntity(StructType, Entity);
						SrcMemory = SparseView.GetMemory();
					}
					else
					{
						FStructView FragmentView = EntityView.GetFragmentDataStruct(StructType);
						SrcMemory = FragmentView.GetMemory();
					}

					if (SrcMemory)
					{
						StructType->CopyScriptStruct(FragmentSlots[SlotIdx].GetMutableMemory(), SrcMemory);
					}
				}
				Proxy->SetEntityFragments(NetId, FragmentSlots);

				Subsystem->RegisterEntityArchetypeKey(Entity, ArchetypeKey);

				const FVector EntityPosition = bHasTransform
					? Transforms[EntityIndex].GetTransform().GetLocation()
					: FVector::ZeroVector;
				Subsystem->AddEntityToGrid(Entity, EntityPosition);
			}
		});
}

// --- Stop Observer ---

UArcMassEntityReplicationStopObserver::UArcMassEntityReplicationStopObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcMassEntityReplicatedTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcMassEntityReplicationStopObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcMassEntityNetIdFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddTagRequirement<FArcMassEntityReplicatedTag>(EMassFragmentPresence::All);
	ObserverQuery.RegisterWithProcessor(*this);
}

void UArcMassEntityReplicationStopObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcMassEntityReplicationSubsystem* Subsystem = World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	ObserverQuery.ForEachEntityChunk(Context,
		[Subsystem](FMassExecutionContext& Context)
		{
			const TConstArrayView<FArcMassEntityNetIdFragment> NetIdFragments =
				Context.GetFragmentView<FArcMassEntityNetIdFragment>();

			for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); ++EntityIndex)
			{
				const FArcMassEntityNetIdFragment& NetIdFragment = NetIdFragments[EntityIndex];
				if (NetIdFragment.NetId.IsValid())
				{
					FMassEntityHandle Entity = Context.GetEntity(EntityIndex);
					Subsystem->RemoveEntityFromGrid(Entity);

					AArcMassEntityReplicationProxy* Proxy = Subsystem->FindProxyForEntity(Entity);
					if (Proxy)
					{
						Proxy->RemoveEntity(NetIdFragment.NetId);
					}
					Subsystem->UnregisterEntityNetId(NetIdFragment.NetId);
				}
			}
		});
}
