// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassReplication.h"

#include "MassCommonTypes.h"
#include "MassEntityManager.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"
#include "MassExecutionContext.h"
#include "MassSpawnerSubsystem.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassReplication)

// ---------------------------------------------------------------------------
// Payload format helpers
// We serialize fragments back-to-back:
//   [FragmentPathNameHash : uint32][DataSize : int32][Data : DataSize bytes]
// ---------------------------------------------------------------------------

namespace ArcMassReplicationPrivate
{

void SerializeEntityPayload(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	const FArcMassReplicationConfigFragment& Config,
	TArray<uint8>& OutPayload)
{
	OutPayload.Reset();
	FMemoryWriter Writer(OutPayload);

	for (const UScriptStruct* FragmentType : Config.ReplicatedFragments)
	{
		if (!FragmentType)
		{
			continue;
		}

		FStructView FragmentView = EntityManager.GetFragmentDataStruct(Entity, FragmentType);
		if (!FragmentView.IsValid())
		{
			continue;
		}

		// Write type identifier (hash of struct path name)
		const uint32 TypeHash = GetTypeHash(FragmentType->GetPathName());
		Writer << const_cast<uint32&>(TypeHash);

		// Serialize fragment into a temp buffer to get size
		TArray<uint8> FragmentBuffer;
		FMemoryWriter FragWriter(FragmentBuffer);
		const_cast<UScriptStruct*>(FragmentType)->SerializeItem(FragWriter, FragmentView.GetMemory(), nullptr);

		// Write size + data
		int32 Size = FragmentBuffer.Num();
		Writer << Size;
		Writer.Serialize(FragmentBuffer.GetData(), Size);
	}
}

void DeserializeEntityPayload(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	const TArray<uint8>& Payload,
	const FArcMassReplicationConfigFragment* Config)
{
	if (Payload.Num() == 0 || !Config)
	{
		return;
	}

	// Build a lookup from path hash -> UScriptStruct
	TMap<uint32, const UScriptStruct*> HashToType;
	for (const UScriptStruct* FragmentType : Config->ReplicatedFragments)
	{
		if (FragmentType)
		{
			HashToType.Add(GetTypeHash(FragmentType->GetPathName()), FragmentType);
		}
	}

	FMemoryReader Reader(Payload);

	while (!Reader.AtEnd())
	{
		uint32 TypeHash = 0;
		Reader << TypeHash;

		int32 Size = 0;
		Reader << Size;

		const UScriptStruct** FoundType = HashToType.Find(TypeHash);
		if (FoundType && *FoundType && Size > 0)
		{
			FStructView FragmentView = EntityManager.GetFragmentDataStruct(Entity, *FoundType);
			if (FragmentView.IsValid())
			{
				TArray<uint8> FragmentBuffer;
				FragmentBuffer.SetNumUninitialized(Size);
				Reader.Serialize(FragmentBuffer.GetData(), Size);

				FMemoryReader FragReader(FragmentBuffer);
				const_cast<UScriptStruct*>(*FoundType)->SerializeItem(FragReader, FragmentView.GetMemory(), nullptr);
				continue;
			}
		}

		// Skip unknown fragment data
		if (Size > 0)
		{
			Reader.Seek(Reader.Tell() + Size);
		}
	}
}

} // namespace ArcMassReplicationPrivate

// ---------------------------------------------------------------------------
// FArcMassReplicatedEntityArray — FastArray client callbacks
// ---------------------------------------------------------------------------

void FArcMassReplicatedEntityArray::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
	if (!OwnerProxy)
	{
		return;
	}

	UWorld* World = OwnerProxy->GetWorld();
	UArcMassReplicationSubsystem* Subsystem = World ? World->GetSubsystem<UArcMassReplicationSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return;
	}

	for (int32 Idx : RemovedIndices)
	{
		if (Items.IsValidIndex(Idx))
		{
			Subsystem->OnClientEntityRemoved(Items[Idx].NetId);
		}
	}
}

void FArcMassReplicatedEntityArray::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	if (!OwnerProxy)
	{
		return;
	}

	UWorld* World = OwnerProxy->GetWorld();
	UArcMassReplicationSubsystem* Subsystem = World ? World->GetSubsystem<UArcMassReplicationSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return;
	}

	for (int32 Idx : AddedIndices)
	{
		if (Items.IsValidIndex(Idx))
		{
			const FArcMassReplicatedEntityItem& Item = Items[Idx];
			Subsystem->OnClientEntityAdded(Item.NetId, Item.Payload);
		}
	}
}

void FArcMassReplicatedEntityArray::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	if (!OwnerProxy)
	{
		return;
	}

	UWorld* World = OwnerProxy->GetWorld();
	UArcMassReplicationSubsystem* Subsystem = World ? World->GetSubsystem<UArcMassReplicationSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return;
	}

	for (int32 Idx : ChangedIndices)
	{
		if (Items.IsValidIndex(Idx))
		{
			const FArcMassReplicatedEntityItem& Item = Items[Idx];
			Subsystem->OnClientEntityChanged(Item.NetId, Item.Payload);
		}
	}
}

// ---------------------------------------------------------------------------
// UArcMassReplicationProxy
// ---------------------------------------------------------------------------

UArcMassReplicationProxy::UArcMassReplicationProxy()
{
	ReplicatedEntities.OwnerProxy = this;
}

void UArcMassReplicationProxy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UArcMassReplicationProxy, ReplicatedEntities);
}

void UArcMassReplicationProxy::StartReplication(ULevel* Level)
{
	Adapter.InitAdapter(this);
	Adapter.StartReplication(Level);
}

void UArcMassReplicationProxy::StopReplication()
{
	if (Adapter.IsReplicating())
	{
		Adapter.StopReplication();
	}
	Adapter.DeinitAdapter();
}

int32 UArcMassReplicationProxy::FindItemIndex(FArcMassNetId NetId) const
{
	for (int32 i = 0; i < ReplicatedEntities.Items.Num(); ++i)
	{
		if (ReplicatedEntities.Items[i].NetId == NetId)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

void UArcMassReplicationProxy::SetEntityPayload(FArcMassNetId NetId, TArray<uint8>&& Payload)
{
	int32 Idx = FindItemIndex(NetId);
	if (Idx != INDEX_NONE)
	{
		// Update existing
		FArcMassReplicatedEntityItem& Item = ReplicatedEntities.Items[Idx];
		Item.Payload = MoveTemp(Payload);
		ReplicatedEntities.MarkItemDirty(Item);
	}
	else
	{
		// Add new
		FArcMassReplicatedEntityItem& NewItem = ReplicatedEntities.Items.AddDefaulted_GetRef();
		NewItem.NetId = NetId;
		NewItem.Payload = MoveTemp(Payload);
		ReplicatedEntities.MarkItemDirty(NewItem);
	}
}

void UArcMassReplicationProxy::RemoveEntity(FArcMassNetId NetId)
{
	int32 Idx = FindItemIndex(NetId);
	if (Idx != INDEX_NONE)
	{
		ReplicatedEntities.Items.RemoveAtSwap(Idx);
		ReplicatedEntities.MarkArrayDirty();
	}
}

// ---------------------------------------------------------------------------
// UArcMassReplicationSubsystem
// ---------------------------------------------------------------------------

void UArcMassReplicationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	NextNetIdValue = 1;
}

void UArcMassReplicationSubsystem::Deinitialize()
{
	if (Proxy && Proxy->IsReplicating())
	{
		Proxy->StopReplication();
	}
	Proxy = nullptr;

	NetIdToEntity.Empty();
	EntityToNetId.Empty();
	Super::Deinitialize();
}

FArcMassNetId UArcMassReplicationSubsystem::AllocateNetId()
{
	return FArcMassNetId(NextNetIdValue++);
}

void UArcMassReplicationSubsystem::RegisterEntity(FArcMassNetId NetId, FMassEntityHandle Entity)
{
	check(NetId.IsValid());
	NetIdToEntity.Add(NetId, Entity);
	EntityToNetId.Add(Entity, NetId);
}

void UArcMassReplicationSubsystem::UnregisterEntity(FArcMassNetId NetId)
{
	if (const FMassEntityHandle* Entity = NetIdToEntity.Find(NetId))
	{
		EntityToNetId.Remove(*Entity);
		NetIdToEntity.Remove(NetId);
	}

	// Remove from the proxy's FastArray
	if (Proxy)
	{
		Proxy->RemoveEntity(NetId);
	}
}

FMassEntityHandle UArcMassReplicationSubsystem::FindEntity(FArcMassNetId NetId) const
{
	const FMassEntityHandle* Found = NetIdToEntity.Find(NetId);
	return Found ? *Found : FMassEntityHandle();
}

FArcMassNetId UArcMassReplicationSubsystem::FindNetId(FMassEntityHandle Entity) const
{
	const FArcMassNetId* Found = EntityToNetId.Find(Entity);
	return Found ? *Found : FArcMassNetId();
}

UArcMassReplicationProxy* UArcMassReplicationSubsystem::GetOrCreateProxy()
{
	if (Proxy)
	{
		return Proxy;
	}

	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}

	Proxy = NewObject<UArcMassReplicationProxy>(this);
	Proxy->StartReplication(World->PersistentLevel);

	return Proxy;
}

void UArcMassReplicationSubsystem::OnClientEntityAdded(FArcMassNetId NetId, const TArray<uint8>& Payload)
{
	FMassEntityHandle Entity = FindEntity(NetId);
	if (Entity.IsSet())
	{
		// Entity already exists on client — just apply new data
		ApplyPayloadToEntity(Entity, Payload);
		return;
	}

	// Entity doesn't exist yet on client.
	// TODO: spawn entity from template info embedded in payload or from a known config.
}

void UArcMassReplicationSubsystem::OnClientEntityChanged(FArcMassNetId NetId, const TArray<uint8>& Payload)
{
	FMassEntityHandle Entity = FindEntity(NetId);
	if (Entity.IsSet())
	{
		ApplyPayloadToEntity(Entity, Payload);
	}
}

void UArcMassReplicationSubsystem::OnClientEntityRemoved(FArcMassNetId NetId)
{
	FMassEntityHandle Entity = FindEntity(NetId);
	if (!Entity.IsSet())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);
	if (EntityManager.IsEntityValid(Entity))
	{
		EntityManager.DestroyEntity(Entity);
	}

	UnregisterEntity(NetId);
}

void UArcMassReplicationSubsystem::ApplyPayloadToEntity(FMassEntityHandle Entity, const TArray<uint8>& Payload)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);
	if (!EntityManager.IsEntityValid(Entity))
	{
		return;
	}

	const FArcMassReplicationConfigFragment* Config =
		EntityManager.GetConstSharedFragmentDataPtr<FArcMassReplicationConfigFragment>(Entity);

	ArcMassReplicationPrivate::DeserializeEntityPayload(EntityManager, Entity, Payload, Config);
}

// ---------------------------------------------------------------------------
// UArcMassNetIdAssignObserver
// ---------------------------------------------------------------------------

UArcMassNetIdAssignObserver::UArcMassNetIdAssignObserver()
	: ObserverQuery{*this}
{
	ObservedType = FArcMassReplicatedTag::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Add;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UArcMassNetIdAssignObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcMassNetIdFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddTagRequirement<FArcMassReplicatedTag>(EMassFragmentPresence::All);
}

void UArcMassNetIdAssignObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcMassReplicationSubsystem* Subsystem = World->GetSubsystem<UArcMassReplicationSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	ObserverQuery.ForEachEntityChunk(EntityManager, Context,
		[Subsystem](FMassExecutionContext& Context)
		{
			const int32 NumEntities = Context.GetNumEntities();
			TArrayView<FArcMassNetIdFragment> NetIdList = Context.GetMutableFragmentView<FArcMassNetIdFragment>();

			for (int32 Idx = 0; Idx < NumEntities; ++Idx)
			{
				FArcMassNetIdFragment& NetIdFragment = NetIdList[Idx];

				if (!NetIdFragment.NetId.IsValid())
				{
					NetIdFragment.NetId = Subsystem->AllocateNetId();
				}

				FMassEntityHandle Entity = Context.GetEntity(Idx);
				Subsystem->RegisterEntity(NetIdFragment.NetId, Entity);
			}
		});
}

// ---------------------------------------------------------------------------
// UArcMassReplicationCollectProcessor
// ---------------------------------------------------------------------------

UArcMassReplicationCollectProcessor::UArcMassReplicationCollectProcessor()
	: EntityQuery{*this}
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::UpdateWorldFromMass);
}

void UArcMassReplicationCollectProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcMassNetIdFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FArcMassReplicatedTag>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FArcMassReplicationConfigFragment>(EMassFragmentPresence::All);
}

void UArcMassReplicationCollectProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcMassReplicationSubsystem* Subsystem = World->GetSubsystem<UArcMassReplicationSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	UArcMassReplicationProxy* Proxy = Subsystem->GetOrCreateProxy();
	if (!Proxy)
	{
		return;
	}

	// Serialize each entity's replicated fragments into the proxy's FastArray.
	// Simplest approach: full re-serialize every tick.
	EntityQuery.ForEachEntityChunk(EntityManager, Context,
		[&EntityManager, Proxy](FMassExecutionContext& Context)
		{
			const int32 NumEntities = Context.GetNumEntities();
			TConstArrayView<FArcMassNetIdFragment> NetIdList = Context.GetFragmentView<FArcMassNetIdFragment>();

			for (int32 Idx = 0; Idx < NumEntities; ++Idx)
			{
				const FArcMassNetIdFragment& NetIdFragment = NetIdList[Idx];
				if (!NetIdFragment.NetId.IsValid())
				{
					continue;
				}

				FMassEntityHandle Entity = Context.GetEntity(Idx);
				const FArcMassReplicationConfigFragment* Config =
					EntityManager.GetConstSharedFragmentDataPtr<FArcMassReplicationConfigFragment>(Entity);
				if (!Config)
				{
					continue;
				}

				TArray<uint8> Payload;
				ArcMassReplicationPrivate::SerializeEntityPayload(EntityManager, Entity, *Config, Payload);
				Proxy->SetEntityPayload(NetIdFragment.NetId, MoveTemp(Payload));
			}
		});
}

// ---------------------------------------------------------------------------
// UArcMassReplicationTrait
// ---------------------------------------------------------------------------

void UArcMassReplicationTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FArcMassNetIdFragment>();
	BuildContext.AddTag<FArcMassReplicatedTag>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct& SharedFragment = EntityManager.GetOrCreateConstSharedFragment(ReplicationConfig);
	BuildContext.AddConstSharedFragment(SharedFragment);
}
