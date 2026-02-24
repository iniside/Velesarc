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
// UArcMassReplicationProxy
// ---------------------------------------------------------------------------

void UArcMassReplicationProxy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UArcMassReplicationProxy, NetId);
	DOREPLIFETIME(UArcMassReplicationProxy, Payload);
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
	// Stop replication on all proxies
	for (auto& [Id, Proxy] : Proxies)
	{
		if (Proxy && Proxy->IsReplicating())
		{
			Proxy->StopReplication();
		}
	}
	Proxies.Empty();

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

	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	// Create a replication proxy for this entity (server only)
	UArcMassReplicationProxy* Proxy = NewObject<UArcMassReplicationProxy>(this);
	Proxy->NetId = NetId;
	Proxies.Add(NetId.GetValue(), Proxy);

	ULevel* Level = World->PersistentLevel;
	Proxy->StartReplication(Level);
}

void UArcMassReplicationSubsystem::UnregisterEntity(FArcMassNetId NetId)
{
	if (const FMassEntityHandle* Entity = NetIdToEntity.Find(NetId))
	{
		EntityToNetId.Remove(*Entity);
		NetIdToEntity.Remove(NetId);
	}

	if (TObjectPtr<UArcMassReplicationProxy>* ProxyPtr = Proxies.Find(NetId.GetValue()))
	{
		UArcMassReplicationProxy* Proxy = *ProxyPtr;
		if (Proxy)
		{
			Proxy->StopReplication();
			Proxy->MarkAsGarbage();
		}
		Proxies.Remove(NetId.GetValue());
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

UArcMassReplicationProxy* UArcMassReplicationSubsystem::FindProxy(FArcMassNetId NetId) const
{
	const TObjectPtr<UArcMassReplicationProxy>* Found = Proxies.Find(NetId.GetValue());
	return Found ? *Found : nullptr;
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

	// Serialize each entity's replicated fragments into its proxy.
	// Simplest approach: full re-serialize every tick.
	EntityQuery.ForEachEntityChunk(EntityManager, Context,
		[&EntityManager, Subsystem](FMassExecutionContext& Context)
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

				UArcMassReplicationProxy* Proxy = Subsystem->FindProxy(NetIdFragment.NetId);
				if (!Proxy)
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

				ArcMassReplicationPrivate::SerializeEntityPayload(EntityManager, Entity, *Config, Proxy->Payload);
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
