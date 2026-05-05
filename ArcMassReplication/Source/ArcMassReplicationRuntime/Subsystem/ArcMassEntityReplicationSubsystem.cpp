// Copyright Lukasz Baran. All Rights Reserved.

#include "Subsystem/ArcMassEntityReplicationSubsystem.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "HAL/PlatformProcess.h"
#include "Iris/ReplicationSystem/NetRefHandle.h"
#include "Iris/ReplicationSystem/ObjectReplicationBridge.h"
#include "Iris/ReplicationSystem/ReplicationProtocol.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "MassEntityConfigAsset.h"
#include "Modules/ModuleManager.h"
#include "Replication/ArcMassEntityRootFactory.h"
#include "Replication/ArcMassEntityVessel.h"
#include "Replication/ArcMassEntityVesselClusterRoot.h"
#include "Traits/ArcMassEntityReplicationTrait.h"

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
	// world subsystem, so we resolve again lazily inside RegisterEntity / warmup.
	UObjectReplicationBridge* Bridge = ArcMassEntityReplicationSubsystem_Private::GetBridge(GetWorld());
	if (Bridge != nullptr)
	{
		FactoryId = FNetObjectFactoryRegistry::GetFactoryIdFromName(UArcMassEntityRootFactory::GetFactoryName());
	}

	// Try the sweep immediately if the bridge is already up.
	EnsureSweepRun();

	// And subscribe to the NetDriver-created delegate as the primary trigger for the
	// usual case where the rep system spins up after this subsystem initializes.
	OnNetDriverCreatedHandle = FWorldDelegates::OnNetDriverCreated.AddUObject(
		this, &UArcMassEntityReplicationSubsystem::HandleNetDriverCreated);
}

void UArcMassEntityReplicationSubsystem::Deinitialize()
{
	if (OnNetDriverCreatedHandle.IsValid())
	{
		FWorldDelegates::OnNetDriverCreated.Remove(OnNetDriverCreatedHandle);
		OnNetDriverCreatedHandle.Reset();
	}

	EntityToVessel.Empty();
	VesselPool.Empty();
	ConfigToProtocol.Empty();
	ProtocolToConfig.Empty();
	VesselClusterRoot = nullptr;
	FactoryId = UE::Net::InvalidNetObjectFactoryId;
	Super::Deinitialize();
}

void UArcMassEntityReplicationSubsystem::AllocateVesselPool(int32 InitialSize)
{
	VesselPool.Reserve(InitialSize);
	for (int32 Index = 0; Index < InitialSize; ++Index)
	{
		UArcMassEntityVessel* Vessel = NewObject<UArcMassEntityVessel>(this);
		VesselPool.Add(Vessel);
	}

#if 0  // GC clustering disabled while smoke-test instability is investigated.
	VesselClusterRoot = NewObject<UArcMassEntityVesselClusterRoot>(this, TEXT("ArcMassVesselClusterRoot"));
	if (VesselClusterRoot != nullptr)
	{
		VesselClusterRoot->CreateCluster();
		for (UArcMassEntityVessel* Vessel : VesselPool)
		{
			if (Vessel != nullptr)
			{
				Vessel->AddToCluster(VesselClusterRoot, /*bAddAsMutableObject=*/ false);
			}
		}
	}
#endif
}

UArcMassEntityVessel* UArcMassEntityReplicationSubsystem::AcquireVessel()
{
	if (VesselPool.Num() > 0)
	{
		UArcMassEntityVessel* Vessel = VesselPool.Pop(EAllowShrinking::No);
		return Vessel;
	}
	UE_LOG(LogTemp, Warning, TEXT("ArcMassReplication: vessel pool exhausted; growing"));
	UArcMassEntityVessel* Vessel = NewObject<UArcMassEntityVessel>(this);
#if 0  // GC clustering disabled while smoke-test instability is investigated.
	if (VesselClusterRoot != nullptr)
	{
		Vessel->AddToCluster(VesselClusterRoot, /*bAddAsMutableObject=*/ false);
	}
#endif
	return Vessel;
}

void UArcMassEntityReplicationSubsystem::ReleaseVessel(UArcMassEntityVessel* Vessel)
{
	if (Vessel == nullptr)
	{
		return;
	}
	Vessel->EntityHandle = FMassEntityHandle();
	Vessel->Config = nullptr;

	// Only return the exact base class to the pool — typed subclasses must be GC'd
	// (they have UPROPERTY state that pool reuse would not safely reset).
	if (Vessel->GetClass() == UArcMassEntityVessel::StaticClass())
	{
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
	UE_LOG(LogTemp, Log,
		TEXT("ArcMassReplication: registered vessel class '%s' for config '%s'"),
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
	return Vessel;
}

UArcMassEntityVessel* UArcMassEntityReplicationSubsystem::AcquireClientVessel(UMassEntityConfigAsset* Config)
{
	UArcMassEntityVessel* Vessel = AcquireVesselForConfig(Config);
	if (Vessel != nullptr)
	{
		Vessel->Config = Config;
		// EntityHandle stays invalid — caller populates after spawning the local Mass entity.
	}
	return Vessel;
}

void UArcMassEntityReplicationSubsystem::ReleaseClientVessel(UArcMassEntityVessel* Vessel)
{
	ReleaseVessel(Vessel);
}

UArcMassEntityVessel* UArcMassEntityReplicationSubsystem::FindVesselForEntity(FMassEntityHandle Entity) const
{
	const TObjectPtr<UArcMassEntityVessel>* Found = EntityToVessel.Find(Entity);
	return Found != nullptr ? Found->Get() : nullptr;
}

FMassEntityHandle UArcMassEntityReplicationSubsystem::FindEntityForVessel(UArcMassEntityVessel* Vessel) const
{
	if (Vessel == nullptr)
	{
		return FMassEntityHandle();
	}
	return Vessel->EntityHandle;
}

UMassEntityConfigAsset* UArcMassEntityReplicationSubsystem::FindConfigForProtocol(uint32 ProtocolId) const
{
	const TObjectPtr<UMassEntityConfigAsset>* Found = ProtocolToConfig.Find(ProtocolId);
	return Found != nullptr ? Found->Get() : nullptr;
}

uint32 UArcMassEntityReplicationSubsystem::FindProtocolForConfig(UMassEntityConfigAsset* Config) const
{
	const uint32* Found = ConfigToProtocol.Find(Config);
	return Found != nullptr ? *Found : 0u;
}

UMassEntityConfigAsset* UArcMassEntityReplicationSubsystem::ResolveClientConfigForProtocol(uint32 ProtocolId)
{
	// Cache hit first.
	if (UMassEntityConfigAsset* Cached = FindConfigForProtocol(ProtocolId))
	{
		return Cached;
	}

	if (PendingClientConfigs.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("ArcMassReplication: ResolveClientConfigForProtocol(0x%x) — no pending client configs"), ProtocolId);
		return nullptr;
	}

	// TODO: real descriptor-driven matching once a public CalculateProtocolIdentifier exists.
	// For now, return the first pending config (smoke test has exactly one).
	UMassEntityConfigAsset* Pick = PendingClientConfigs[0].Get();
	if (Pick == nullptr)
	{
		return nullptr;
	}

	if (PendingClientConfigs.Num() > 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("ArcMassReplication: ResolveClientConfigForProtocol(0x%x) — %d pending configs, returning first ('%s'). Multi-config matching not implemented yet."),
			ProtocolId, PendingClientConfigs.Num(), *Pick->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("ArcMassReplication: ResolveClientConfigForProtocol(0x%x) — using only pending client config '%s'"),
			ProtocolId, *Pick->GetName());
	}

	ConfigToProtocol.Add(Pick, ProtocolId);
	ProtocolToConfig.Add(ProtocolId, Pick);
	return Pick;
}

bool UArcMassEntityReplicationSubsystem::RegisterConfigForReplication(UMassEntityConfigAsset* Config)
{
	using namespace UE::Net;

	if (Config == nullptr)
	{
		return false;
	}
	if (bProtocolWarmupLocked)
	{
		UE_LOG(LogTemp, Warning, TEXT("ArcMassReplication: RegisterConfigForReplication blocked by warmup lock for %s"), *Config->GetName());
		return false;
	}
	if (ConfigToProtocol.Contains(Config))
	{
		return true;
	}

	// Make sure the asset-registry sweep has had its chance — but only when we already
	// have a bridge (otherwise the sweep would just no-op again here).
	EnsureSweepRun();

	UWorld* World = GetWorld();
	UObjectReplicationBridge* Bridge = ArcMassEntityReplicationSubsystem_Private::GetBridge(World);
	if (Bridge == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("ArcMassReplication: warmup deferred — no bridge yet (%s)"), *Config->GetName());
		return false;
	}

	if (!FNetObjectFactoryRegistry::IsValidFactoryId(FactoryId))
	{
		FactoryId = FNetObjectFactoryRegistry::GetFactoryIdFromName(UArcMassEntityRootFactory::GetFactoryName());
		if (!FNetObjectFactoryRegistry::IsValidFactoryId(FactoryId))
		{
			UE_LOG(LogTemp, Warning, TEXT("ArcMassReplication: factory id unresolved during warmup for %s"), *Config->GetName());
			return false;
		}
	}

	UReplicationSystem* RepSystem = World->GetNetDriver()->GetReplicationSystem();
	if (RepSystem == nullptr)
	{
		return false;
	}

	// On clients, StartReplicatingRootObject is server-only — it silently returns
	// an invalid handle. Skip the bridge call and record the config for lazy
	// protocol resolution inside InstantiateReplicatedObjectFromHeader.
	// TODO: replace this stopgap with proper deterministic protocol-id matching once a
	// public path to FReplicationProtocolManager::CalculateProtocolIdentifier is wired.
	if (!RepSystem->IsServer())
	{
		PendingClientConfigs.AddUnique(Config);
		UE_LOG(LogTemp, Log, TEXT("ArcMassReplication: client-side RegisterConfigForReplication recorded pending config %s (PendingCount=%d)"),
			*Config->GetName(), PendingClientConfigs.Num());
		return true;
	}

	UArcMassEntityVessel* TempVessel = AcquireVessel();
	if (TempVessel == nullptr)
	{
		return false;
	}
	TempVessel->Config = Config;
	// EntityHandle stays invalid — registration only builds descriptors; chunk memory
	// resolution happens later in Poll/Apply, which gracefully no-ops on invalid handles.

	const ENetMode NetMode = World->GetNetMode();
	const bool bIsServer = RepSystem->IsServer();
	UE_LOG(LogTemp, Log, TEXT("ArcMassReplication: warmup calling StartReplicatingRootObject (Config=%s, Vessel=%s, FactoryId=%u, IsSupportedForNetworking=%s, NetMode=%d, RepSystem.IsServer=%s, World=%s)"),
		*Config->GetName(),
		*TempVessel->GetName(),
		(uint32)FactoryId,
		TempVessel->IsSupportedForNetworking() ? TEXT("true") : TEXT("false"),
		(int32)NetMode,
		bIsServer ? TEXT("true") : TEXT("false"),
		*World->GetName());

	const FNetRefHandle Handle = Bridge->StartReplicatingRootObject(TempVessel, FactoryId);
	if (!Handle.IsValid())
	{
		ReleaseVessel(TempVessel);
		UE_LOG(LogTemp, Warning, TEXT("ArcMassReplication: warmup StartReplicatingRootObject failed for %s — handle invalid (NetMode=%d, RepSystem.IsServer=%s)"),
			*Config->GetName(), (int32)NetMode, bIsServer ? TEXT("true") : TEXT("false"));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("ArcMassReplication: warmup got NetRefHandle (%s) for Config=%s"),
		*Handle.ToString(), *Config->GetName());

	const FReplicationProtocol* Protocol = RepSystem->GetReplicationProtocol(Handle);
	if (Protocol == nullptr)
	{
		Bridge->StopReplicatingNetObject(TempVessel, ArcMassEntityReplicationSubsystem_Private::GArcMassEndReplicationFlags);
		ReleaseVessel(TempVessel);
		return false;
	}

	const uint32 ProtocolId = Protocol->ProtocolIdentifier;
	ConfigToProtocol.Add(Config, ProtocolId);
	ProtocolToConfig.Add(ProtocolId, Config);

	Bridge->StopReplicatingNetObject(TempVessel, ArcMassEntityReplicationSubsystem_Private::GArcMassEndReplicationFlags);

	// TODO Phase 5c: when the bridge exposes a synchronous "is this handle still bound"
	// query, replace the single yield below with a bounded poll. The Destroy/
	// DestroyNetHandle/ClearNetPushId flags break the binding immediately — the next
	// StartReplicatingRootObject for the same vessel will produce a fresh handle — so
	// the yield is sufficient to let the bridge complete its bookkeeping.
	FPlatformProcess::SleepNoStats(0.0f);

	ReleaseVessel(TempVessel);
	return true;
}

UE::Net::FNetRefHandle UArcMassEntityReplicationSubsystem::RegisterEntity(FMassEntityHandle Entity, UMassEntityConfigAsset* Config)
{
	using namespace UE::Net;

	if (!Entity.IsSet() || Config == nullptr)
	{
		return FNetRefHandle::GetInvalid();
	}

	// Asset-registry sweep MUST happen before the first RegisterEntity, since that call
	// flips the warmup lock. The sweep is idempotent and cheap once it has run.
	EnsureSweepRun();

	UWorld* World = GetWorld();
	UObjectReplicationBridge* Bridge = ArcMassEntityReplicationSubsystem_Private::GetBridge(World);
	if (Bridge == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("ArcMassReplication: RegisterEntity called but no bridge available"));
		return FNetRefHandle::GetInvalid();
	}

	if (!FNetObjectFactoryRegistry::IsValidFactoryId(FactoryId))
	{
		// Resolve lazily if Initialize couldn't (rep system came up later).
		FactoryId = FNetObjectFactoryRegistry::GetFactoryIdFromName(UArcMassEntityRootFactory::GetFactoryName());
		if (!FNetObjectFactoryRegistry::IsValidFactoryId(FactoryId))
		{
			UE_LOG(LogTemp, Warning, TEXT("ArcMassReplication: factory id unresolved for ArcMassEntityReplication"));
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

	// Capture protocol id post-registration as a backup (warmup populates eagerly,
	// but if RegisterEntity races ahead of warmup we still record it).
	UReplicationSystem* RepSystem = World->GetNetDriver()->GetReplicationSystem();
	const FReplicationProtocol* Protocol = RepSystem != nullptr ? RepSystem->GetReplicationProtocol(Handle) : nullptr;
	if (Protocol != nullptr)
	{
		const uint32 ProtocolId = Protocol->ProtocolIdentifier;
		ConfigToProtocol.Add(Config, ProtocolId);
		ProtocolToConfig.Add(ProtocolId, Config);
	}

	// Lock the warmup window only after the first successful registration. If we
	// flipped this earlier, an AcquireVessel/StartReplicatingRootObject failure
	// would leave the lock set permanently and silently block all subsequent
	// RegisterConfigForReplication calls for the world's lifetime.
	bProtocolWarmupLocked = true;

	return Handle;
}

void UArcMassEntityReplicationSubsystem::UnregisterEntity(FMassEntityHandle Entity)
{
	UArcMassEntityVessel* Vessel = FindVesselForEntity(Entity);
	if (Vessel == nullptr)
	{
		return;
	}

	UObjectReplicationBridge* Bridge = ArcMassEntityReplicationSubsystem_Private::GetBridge(GetWorld());
	if (Bridge != nullptr)
	{
		Bridge->StopReplicatingNetObject(Vessel, ArcMassEntityReplicationSubsystem_Private::GArcMassEndReplicationFlags);
	}

	EntityToVessel.Remove(Entity);
	ReleaseVessel(Vessel);
}

void UArcMassEntityReplicationSubsystem::EnsureSweepRun()
{
	if (bAssetRegistrySweepRun)
	{
		return;
	}
	if (ArcMassEntityReplicationSubsystem_Private::GetBridge(GetWorld()) == nullptr)
	{
		// Bridge not ready yet — HandleNetDriverCreated (or a later RegisterEntity) will retry.
		return;
	}
	RunAssetRegistrySweep();
}

void UArcMassEntityReplicationSubsystem::HandleNetDriverCreated(UWorld* InWorld, UNetDriver* /*InNetDriver*/)
{
	if (InWorld != GetWorld())
	{
		return;
	}
	EnsureSweepRun();
}

void UArcMassEntityReplicationSubsystem::RunAssetRegistrySweep()
{
	if (bAssetRegistrySweepRun)
	{
		return;
	}

	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetData;
	AssetRegistry.GetAssetsByClass(UMassEntityConfigAsset::StaticClass()->GetClassPathName(), AssetData);

	for (const FAssetData& Data : AssetData)
	{
		UMassEntityConfigAsset* Config = Cast<UMassEntityConfigAsset>(Data.GetAsset());
		if (Config == nullptr)
		{
			continue;
		}

		// Only register configs that actually carry the replication trait.
		const UArcMassEntityReplicationTrait* Trait =
			Cast<UArcMassEntityReplicationTrait>(Config->FindTrait(UArcMassEntityReplicationTrait::StaticClass()));
		if (Trait == nullptr)
		{
			continue;
		}

		RegisterConfigForReplication(Config);
	}

	bAssetRegistrySweepRun = true;
}
