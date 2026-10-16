// Copyright Lukasz Baran. All Rights Reserved.

#include "Subsystem/ArcMassEntityReplicationSubsystem.h"

#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "Iris/ReplicationSystem/NetRefHandle.h"
#include "Iris/ReplicationSystem/ObjectReplicationBridge.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "MassEntityConfigAsset.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Replication/ArcMassEntityRootFactory.h"
#include "Replication/ArcMassEntityVessel.h"

namespace ArcMassEntityReplicationSubsystem_Private
{
	constexpr int32 GArcMassVesselInitialPoolSize = 256;

	UObjectReplicationBridge* GetBridge(const UWorld* World)
	{
		if (World == nullptr)
		{
			return nullptr;
		}
		UNetDriver* NetDriver = World->GetNetDriver();
		if (NetDriver == nullptr)
		{
			return nullptr;
		}
		UReplicationSystem* RepSystem = NetDriver->GetReplicationSystem();
		if (RepSystem == nullptr)
		{
			return nullptr;
		}
		return RepSystem->GetReplicationBridgeAs<UObjectReplicationBridge>();
	}

	constexpr EEndReplicationFlags GArcMassEndReplicationFlags =
		EEndReplicationFlags::Destroy
		| EEndReplicationFlags::DestroyNetHandle
		| EEndReplicationFlags::ClearNetPushId;
}

bool UArcMassEntityReplicationSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	UWorld* World = Cast<UWorld>(Outer);
	if (World == nullptr)
	{
		return false;
	}
	return World->IsGameWorld();
}

void UArcMassEntityReplicationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	using namespace UE::Net;

	Super::Initialize(Collection);
	AllocateVesselPool(ArcMassEntityReplicationSubsystem_Private::GArcMassVesselInitialPoolSize);

	// Best-effort early resolution; the rep system is usually created later than the
	// world subsystem, so we resolve again lazily inside RegisterEntity.
	UObjectReplicationBridge* Bridge = ArcMassEntityReplicationSubsystem_Private::GetBridge(GetWorld());
	if (Bridge != nullptr)
	{
		FactoryId = FNetObjectFactoryRegistry::GetFactoryIdFromName(UArcMassEntityRootFactory::GetFactoryName());
	}
}

void UArcMassEntityReplicationSubsystem::Deinitialize()
{
	// Server sweep: tell Iris to drop any vessels still registered as net objects.
	// UnregisterEntity handles the per-entity path, but bulk teardown (level
	// transition, world destruction) can run before all entities are destroyed —
	// leaving Iris holding dangling NetRefHandles pointing at vessels whose map
	// entries are about to be Empty()'d. Guard on bRegisteredWithIris so only
	// vessels actually handed to Iris are stopped. The bridge itself may already
	// be gone at teardown — skip the sweep if so.
	UObjectReplicationBridge* Bridge = ArcMassEntityReplicationSubsystem_Private::GetBridge(GetWorld());
	if (Bridge != nullptr)
	{
		for (const TPair<FMassEntityHandle, TObjectPtr<UArcMassEntityVessel>>& Pair : EntityToVessel)
		{
			UArcMassEntityVessel* Vessel = Pair.Value.Get();
			if (Vessel != nullptr && Vessel->bRegisteredWithIris)
			{
				Bridge->StopReplicatingNetRefHandle(
					Vessel->NetRefHandle,
					ArcMassEntityReplicationSubsystem_Private::GArcMassEndReplicationFlags);
			}
		}
	}

	// Client safety-net sweep: DetachedFromReplication does not fire for every
	// teardown ordering; destroy any surviving local entities and release the
	// vessels so nothing leaks past subsystem shutdown.
	for (const TObjectPtr<UArcMassEntityVessel>& ClientVessel : ClientVessels)
	{
		UArcMassEntityVessel* Vessel = ClientVessel.Get();
		if (Vessel != nullptr)
		{
			DestroyLocalEntityForVessel(Vessel);
			Vessel->MarkAsGarbage();
		}
	}
	ClientVessels.Empty();

	EntityToVessel.Empty();
	VesselPool.Empty();
	FactoryId = UE::Net::InvalidNetObjectFactoryId;
	Super::Deinitialize();
}

void UArcMassEntityReplicationSubsystem::AllocateVesselPool(int32 InitialSize)
{
	VesselPool.Reserve(InitialSize);
	for (int32 Index = 0; Index < InitialSize; ++Index)
	{
		UArcMassEntityVessel* Vessel = NewObject<UArcMassEntityVessel>(this);
		Vessel->bInPool = true;
		VesselPool.Add(Vessel);
	}
}

UArcMassEntityVessel* UArcMassEntityReplicationSubsystem::AcquireVessel()
{
	if (VesselPool.Num() > 0)
	{
		UArcMassEntityVessel* Vessel = VesselPool.Pop(EAllowShrinking::No);
		Vessel->bInPool = false;
		Vessel->bForceDirty = true;
		return Vessel;
	}
	UE_LOGF(LogTemp, Warning,"ArcMassReplication: vessel pool exhausted; growing");
	UArcMassEntityVessel* Vessel = NewObject<UArcMassEntityVessel>(this);
	Vessel->bForceDirty = true;
	return Vessel;
}

void UArcMassEntityReplicationSubsystem::ReleaseVessel(UArcMassEntityVessel* Vessel)
{
	if (Vessel == nullptr)
	{
		return;
	}
	// Double-free guard: a second release would push the same vessel onto the pool
	// twice, so two later AcquireVessel calls would hand the same vessel to two
	// entities and cross-contaminate their replicated state.
	if (Vessel->bInPool)
	{
		UE_LOGF(LogTemp, Warning,"ArcMassReplication: ReleaseVessel called on already-pooled vessel '%ls'; ignoring",
			*GetNameSafe(Vessel));
		return;
	}
	Vessel->EntityHandle = FMassEntityHandle();
	Vessel->Config = nullptr;
	Vessel->NetRefHandle = UE::Net::FNetRefHandle::GetInvalid();
	Vessel->bRegisteredWithIris = false;
	Vessel->bForceDirty = false;

	// Only return the exact base class to the pool — typed subclasses must be GC'd
	// (they have UPROPERTY state that pool reuse would not safely reset).
	if (Vessel->GetClass() == UArcMassEntityVessel::StaticClass())
	{
		Vessel->bInPool = true;
		VesselPool.Add(Vessel);
	}
	// Otherwise the vessel becomes unreachable and GC reclaims it.
}

void UArcMassEntityReplicationSubsystem::RegisterVesselClassForConfig(
	UMassEntityConfigAsset* Config,
	TSubclassOf<UArcMassEntityVessel> VesselClass)
{
	if (Config == nullptr || VesselClass == nullptr)
	{
		return;
	}
	ConfigToVesselClass.Add(Config, VesselClass);
	UE_LOGF(LogTemp, Log,"ArcMassReplication: registered vessel class '%ls' for config '%ls'",
		*VesselClass->GetName(), *Config->GetName());
}

TSubclassOf<UArcMassEntityVessel> UArcMassEntityReplicationSubsystem::FindVesselClassForConfig(
	UMassEntityConfigAsset* Config) const
{
	const TSubclassOf<UArcMassEntityVessel>* Found = ConfigToVesselClass.Find(Config);
	return Found != nullptr ? *Found : TSubclassOf<UArcMassEntityVessel>();
}

UArcMassEntityVessel* UArcMassEntityReplicationSubsystem::AcquireVesselForConfig(
	UMassEntityConfigAsset* Config)
{
	TSubclassOf<UArcMassEntityVessel> VesselClass = FindVesselClassForConfig(Config);
	if (VesselClass == nullptr)
	{
		// Legacy / un-typed config — fall back to pool-allocated base class.
		return AcquireVessel();
	}
	UArcMassEntityVessel* Vessel = NewObject<UArcMassEntityVessel>(this, VesselClass);
	Vessel->bForceDirty = true;
	return Vessel;
}

UArcMassEntityVessel* UArcMassEntityReplicationSubsystem::AcquireClientVessel(UMassEntityConfigAsset* Config)
{
	// Client vessels are never pooled: Iris owns their lifetime
	// (AllowDestroyInstanceFromRemote), so pool reuse would race remote destruction.
	TSubclassOf<UArcMassEntityVessel> VesselClass = FindVesselClassForConfig(Config);
	if (VesselClass == nullptr)
	{
		VesselClass = UArcMassEntityVessel::StaticClass();
	}
	UArcMassEntityVessel* Vessel = NewObject<UArcMassEntityVessel>(this, VesselClass);
	Vessel->Config = Config;
	Vessel->bForceDirty = true;
	// EntityHandle stays invalid — caller populates after spawning the local Mass entity.
	ClientVessels.Add(Vessel);
	return Vessel;
}

void UArcMassEntityReplicationSubsystem::ReleaseClientVessel(UArcMassEntityVessel* Vessel)
{
	if (Vessel == nullptr)
	{
		return;
	}
	ClientVessels.Remove(Vessel);
	Vessel->MarkAsGarbage();
}

void UArcMassEntityReplicationSubsystem::DestroyLocalEntityForVessel(UArcMassEntityVessel* Vessel)
{
	if (Vessel == nullptr || !Vessel->EntityHandle.IsSet())
	{
		return;
	}

	UWorld* World = Vessel->GetWorld();
	if (World == nullptr)
	{
		return;
	}

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (EntitySubsystem == nullptr)
	{
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	EntityManager.Defer().DestroyEntity(Vessel->EntityHandle);
	Vessel->EntityHandle = FMassEntityHandle();
}

UArcMassEntityVessel* UArcMassEntityReplicationSubsystem::FindVesselForEntity(FMassEntityHandle Entity) const
{
	const TObjectPtr<UArcMassEntityVessel>* Found = EntityToVessel.Find(Entity);
	return Found != nullptr ? Found->Get() : nullptr;
}

UE::Net::FNetRefHandle UArcMassEntityReplicationSubsystem::RegisterEntity(FMassEntityHandle Entity, UMassEntityConfigAsset* Config)
{
	using namespace UE::Net;

	if (!Entity.IsSet() || Config == nullptr)
	{
		return FNetRefHandle::GetInvalid();
	}

	UWorld* World = GetWorld();
	UObjectReplicationBridge* Bridge = ArcMassEntityReplicationSubsystem_Private::GetBridge(World);
	if (Bridge == nullptr)
	{
		UE_LOGF(LogTemp, Warning,"ArcMassReplication: RegisterEntity called but no bridge available");
		return FNetRefHandle::GetInvalid();
	}

	if (!FNetObjectFactoryRegistry::IsValidFactoryId(FactoryId))
	{
		// Resolve lazily if Initialize couldn't (rep system came up later).
		FactoryId = FNetObjectFactoryRegistry::GetFactoryIdFromName(UArcMassEntityRootFactory::GetFactoryName());
		if (!FNetObjectFactoryRegistry::IsValidFactoryId(FactoryId))
		{
			UE_LOGF(LogTemp, Warning,"ArcMassReplication: factory id unresolved for ArcMassEntityReplication");
			return FNetRefHandle::GetInvalid();
		}
	}

	UArcMassEntityVessel* Vessel = AcquireVesselForConfig(Config);
	if (Vessel == nullptr)
	{
		return FNetRefHandle::GetInvalid();
	}
	Vessel->EntityHandle = Entity;
	Vessel->Config = Config;
	EntityToVessel.Add(Entity, Vessel);

	const FNetRefHandle Handle = Bridge->StartReplicatingRootObject(Vessel, FactoryId);
	if (!Handle.IsValid())
	{
		EntityToVessel.Remove(Entity);
		ReleaseVessel(Vessel);
		return FNetRefHandle::GetInvalid();
	}

	Vessel->NetRefHandle = Handle;
	Vessel->bRegisteredWithIris = true;

	return Handle;
}

void UArcMassEntityReplicationSubsystem::UnregisterEntity(FMassEntityHandle Entity)
{
	UArcMassEntityVessel* Vessel = FindVesselForEntity(Entity);
	if (Vessel == nullptr)
	{
		return;
	}

	// Stop by handle, not by instance — safe even when the instance-to-handle
	// association inside the bridge has already been torn down.
	if (Vessel->bRegisteredWithIris)
	{
		UObjectReplicationBridge* Bridge = ArcMassEntityReplicationSubsystem_Private::GetBridge(GetWorld());
		if (Bridge != nullptr)
		{
			Bridge->StopReplicatingNetRefHandle(Vessel->NetRefHandle, ArcMassEntityReplicationSubsystem_Private::GArcMassEndReplicationFlags);
		}
	}

	EntityToVessel.Remove(Entity);
	ReleaseVessel(Vessel);
}
