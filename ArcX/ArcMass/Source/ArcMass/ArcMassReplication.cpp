// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassReplication.h"

#include "MassCommonTypes.h"
#include "MassEntityManager.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "MassSpawnerSubsystem.h"
#include "Engine/NetDriver.h"
#include "Iris/ReplicationSystem/ObjectReplicationBridge.h"
#include "Iris/ReplicationSystem/NetObjectFactoryRegistry.h"
#include "Net/Iris/ReplicationSystem/ReplicationSystemUtil.h"
#include "Net/Iris/ReplicationSystem/EngineReplicationBridge.h"
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
// UArcMassReplicationProxyFactory
// ---------------------------------------------------------------------------

FName UArcMassReplicationProxyFactory::GetFactoryName()
{
	static const FName Name(TEXT("ArcMassProxyFactory"));
	return Name;
}

TOptional<UNetObjectFactory::FWorldInfoData> UArcMassReplicationProxyFactory::GetWorldInfo(const FWorldInfoContext& Context) const
{
	UArcMassReplicationProxy* Proxy = Cast<UArcMassReplicationProxy>(Context.Instance);
	if (!Proxy)
	{
		return NullOpt;
	}

	FWorldInfoData Data;
	Data.WorldLocation = Proxy->WorldPosition;
	Data.CullDistance = Proxy->CullDistance;
	return Data;
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
	if (bIsReplicating)
	{
		return;
	}

	UWorld* World = Level ? Level->GetWorld() : nullptr;
	if (!World || World->bIsTearingDown)
	{
		return;
	}

	if (World->GetNetDriver() && !World->GetNetDriver()->IsUsingIrisReplication())
	{
		return;
	}

	const FName FactoryName = UArcMassReplicationProxyFactory::GetFactoryName();
	const UE::Net::FNetObjectFactoryId FactoryId = UE::Net::FNetObjectFactoryRegistry::GetFactoryIdFromName(FactoryName);
	if (FactoryId == UE::Net::InvalidNetObjectFactoryId)
	{
		UE_LOG(LogTemp, Error, TEXT("ArcMassReplicationProxy: Factory '%s' not registered"), *FactoryName.ToString());
		return;
	}

	UE::Net::FReplicationSystemUtil::ForEachServerBridge(World, [&](UEngineReplicationBridge* Bridge)
	{
		UObjectReplicationBridge::FRootObjectReplicationParams Params;
		Params.bNeedsWorldLocationUpdate = true;
		Params.bUseClassConfigDynamicFilter = false;
		Params.bUseExplicitDynamicFilter = true;
		Params.ExplicitDynamicFilterName = TEXT("Spatial");

		const UE::Net::FNetRefHandle RefHandle = Bridge->StartReplicatingRootObject(this, Params, FactoryId);
		if (RefHandle.IsValid())
		{
			bIsReplicating = true;
			Bridge->AddRootObjectToContainerGroup(this, Level);
		}
	});
}

void UArcMassReplicationProxy::StopReplication()
{
	if (!bIsReplicating)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UE::Net::FReplicationSystemUtil::ForEachServerBridge(World, [&](UEngineReplicationBridge* Bridge)
	{
		UE::Net::FNetRefHandle RefHandle = Bridge->GetReplicatedRefHandle(this, UE::Net::EGetRefHandleFlags::EvenIfGarbage);
		if (RefHandle.IsValid())
		{
			constexpr EEndReplicationFlags Flags =
				EEndReplicationFlags::Destroy |
				EEndReplicationFlags::DestroyNetHandle |
				EEndReplicationFlags::ClearNetPushId;
			Bridge->StopReplicatingNetRefHandle(RefHandle, Flags);
		}
	});

	bIsReplicating = false;
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
	DirtyEntities.Empty();
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
		DirtyEntities.Remove(*Entity);
		EntityToNetId.Remove(*Entity);
		NetIdToEntity.Remove(NetId);
	}

	// Remove from the proxy's FastArray
	if (Proxy)
	{
		Proxy->RemoveEntity(NetId);
	}
}

void UArcMassReplicationSubsystem::MarkEntityDirty(FMassEntityHandle Entity)
{
	DirtyEntities.Add(Entity);
}

void UArcMassReplicationSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (DirtyEntities.Num() == 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
	if (!SignalSubsystem)
	{
		return;
	}

	// Batch-signal all dirty entities at once.
	// The flush processor will consume DirtyEntities via TakeDirtyEntities().
	TArray<FMassEntityHandle> Entities = DirtyEntities.Array();
	SignalSubsystem->SignalEntities(UE::ArcMass::Signals::ReplicationDirty, Entities);
}

TStatId UArcMassReplicationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArcMassReplicationSubsystem, STATGROUP_Game);
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
	// Default: origin position, entities within CullDistance are replicated
	Proxy->WorldPosition = FVector::ZeroVector;
	Proxy->CullDistance = 15000.0f;
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
// UArcMassReplicationFlushProcessor
// Signal-based: only runs when entities have been marked dirty via
// MarkEntityDirty(). Consumes the dirty set and serializes only those
// entities into the proxy FastArray.
// ---------------------------------------------------------------------------

UArcMassReplicationFlushProcessor::UArcMassReplicationFlushProcessor()
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::UpdateWorldFromMass);
}

void UArcMassReplicationFlushProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	if (const UWorld* World = GetWorld())
	{
		if (UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>())
		{
			SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::ReplicationDirty);
		}
	}
}

void UArcMassReplicationFlushProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcMassNetIdFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FArcMassReplicatedTag>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FArcMassReplicationConfigFragment>(EMassFragmentPresence::All);
}

void UArcMassReplicationFlushProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
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

	// Consume the accumulated dirty set from the subsystem.
	TSet<FMassEntityHandle> DirtyEntities = Subsystem->TakeDirtyEntities();
	if (DirtyEntities.IsEmpty())
	{
		return;
	}

	// Serialize only the dirty entities into the proxy's FastArray.
	for (const FMassEntityHandle& Entity : DirtyEntities)
	{
		if (!EntityManager.IsEntityValid(Entity))
		{
			continue;
		}

		const FArcMassNetIdFragment* NetIdFragment = EntityManager.GetFragmentDataPtr<FArcMassNetIdFragment>(Entity);
		if (!NetIdFragment || !NetIdFragment->NetId.IsValid())
		{
			continue;
		}

		const FArcMassReplicationConfigFragment* Config =
			EntityManager.GetConstSharedFragmentDataPtr<FArcMassReplicationConfigFragment>(Entity);
		if (!Config)
		{
			continue;
		}

		TArray<uint8> Payload;
		ArcMassReplicationPrivate::SerializeEntityPayload(EntityManager, Entity, *Config, Payload);
		Proxy->SetEntityPayload(NetIdFragment->NetId, MoveTemp(Payload));
	}
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
