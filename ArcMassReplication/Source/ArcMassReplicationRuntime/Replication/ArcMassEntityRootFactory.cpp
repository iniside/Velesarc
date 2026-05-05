// Copyright Lukasz Baran. All Rights Reserved.

#include "Replication/ArcMassEntityRootFactory.h"

#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "Iris/ReplicationSystem/ObjectReferenceCacheFwd.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityTemplate.h"
#include "MassSpawnerSubsystem.h"
#include "Replication/ArcMassEntityVessel.h"
#include "Subsystem/ArcMassEntityReplicationSubsystem.h"

namespace ArcMassEntityRootFactory_Private
{
	// Resolve the world tied to a specific UReplicationSystem by matching it against
	// each world context's NetDriver. This is the correct way to scope per-world
	// state when running multiple worlds (PIE host + client share a process); the
	// previous best-effort first-Game-world walk caused observer reentrancy by
	// landing client-spawned local entities in the server world.
	UWorld* FindWorldForReplicationSystem(const UE::Net::FNetObjectResolveContext& ResolveContext)
	{
		if (GEngine == nullptr || ResolveContext.ReplicationSystem == nullptr)
		{
			return nullptr;
		}

		const UReplicationSystem* TargetRepSystem = ResolveContext.ReplicationSystem;

		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			if (WorldContext.WorldType != EWorldType::Game
				&& WorldContext.WorldType != EWorldType::PIE)
			{
				continue;
			}

			UWorld* World = WorldContext.World();
			if (World == nullptr)
			{
				continue;
			}

			const UNetDriver* NetDriver = World->GetNetDriver();
			if (NetDriver == nullptr)
			{
				continue;
			}

			if (NetDriver->GetReplicationSystem() == TargetRepSystem)
			{
				return World;
			}
		}

		UE_LOG(LogTemp, Warning,
			TEXT("ArcMassReplication: no world matched ReplicationSystem in FindWorldForReplicationSystem"));
		return nullptr;
	}

	// Fallback for paths that don't have an Iris context (DetachedFromReplication
	// when the vessel's UWorld is null). Walks Game/PIE worlds and returns the first
	// one. Logs a warning if multiple match. Should only be hit on teardown paths.
	UWorld* FindGameWorld()
	{
		if (GEngine == nullptr)
		{
			return nullptr;
		}

		UWorld* FirstMatch = nullptr;
		int32 MatchCount = 0;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType != EWorldType::Game
				&& Context.WorldType != EWorldType::PIE)
			{
				continue;
			}

			UWorld* World = Context.World();
			if (World == nullptr)
			{
				continue;
			}

			if (FirstMatch == nullptr)
			{
				FirstMatch = World;
			}
			++MatchCount;
		}

		if (MatchCount > 1)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("ArcMassReplication: FindGameWorld fallback matched %d Game/PIE worlds; picking first (%s)."),
				MatchCount, *FirstMatch->GetName());
		}

		return FirstMatch;
	}

	UArcMassEntityReplicationSubsystem* GetSubsystem()
	{
		UWorld* World = FindGameWorld();
		return World != nullptr ? World->GetSubsystem<UArcMassEntityReplicationSubsystem>() : nullptr;
	}

	UArcMassEntityReplicationSubsystem* GetSubsystemFromObject(const UObject* Object)
	{
		const UWorld* World = Object != nullptr ? Object->GetWorld() : nullptr;
		if (World != nullptr)
		{
			return World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
		}

		return GetSubsystem();
	}

	FMassEntityHandle SpawnLocalEntity(UWorld* World, UMassEntityConfigAsset* Config)
	{
		if (World == nullptr || Config == nullptr)
		{
			return FMassEntityHandle();
		}

		UMassSpawnerSubsystem* SpawnerSubsystem = World->GetSubsystem<UMassSpawnerSubsystem>();
		if (SpawnerSubsystem == nullptr)
		{
			return FMassEntityHandle();
		}

		const FMassEntityTemplate& EntityTemplate = Config->GetOrCreateEntityTemplate(*World);

		TArray<FMassEntityHandle> SpawnedEntities;
		TSharedPtr<FMassEntityManager::FEntityCreationContext> CreationContext =
			SpawnerSubsystem->SpawnEntities(EntityTemplate, 1, SpawnedEntities);
		// Release creation context to finalize spawning (runs observers and deferred commands).
		CreationContext.Reset();

		if (SpawnedEntities.Num() == 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("ArcMassReplication: SpawnLocalEntity got 0 entities for config %s"),
				*Config->GetName());
			return FMassEntityHandle();
		}

		if (!ensureMsgf(SpawnedEntities.Num() == 1,
			TEXT("ArcMassReplication: SpawnLocalEntity expected 1 entity for config %s but got %d. ")
			TEXT("Destroying extras to prevent leak. Config likely has a trait that multiplies spawn count."),
			*Config->GetName(), SpawnedEntities.Num()))
		{
			// Destroy all extras to prevent leaking unowned entities into the world.
			UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
			if (EntitySubsystem != nullptr)
			{
				FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
				for (int32 Index = 1; Index < SpawnedEntities.Num(); ++Index)
				{
					EntityManager.Defer().DestroyEntity(SpawnedEntities[Index]);
				}
			}
		}

		return SpawnedEntities[0];
	}

	void DestroyLocalEntity(UWorld* World, FMassEntityHandle Entity)
	{
		if (World == nullptr || !Entity.IsSet())
		{
			return;
		}

		UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
		if (EntitySubsystem == nullptr)
		{
			return;
		}

		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		EntityManager.Defer().DestroyEntity(Entity);
	}
}

FName UArcMassEntityRootFactory::GetFactoryName()
{
	static const FName Name(TEXT("ArcMassEntityReplication"));
	return Name;
}

TOptional<UArcMassEntityRootFactory::FWorldInfoData> UArcMassEntityRootFactory::GetWorldInfo(
	const FWorldInfoContext& Context) const
{
	return NullOpt;
}

void UArcMassEntityRootFactory::FillRootObjectReplicationParams(
	const UE::Net::FRootObjectReplicationParamsContext& Context,
	UE::Net::FRootObjectReplicationParams& OutParams)
{
	OutParams.bNeedsWorldLocationUpdate = false;
	OutParams.bUseClassConfigDynamicFilter = false;
}

UNetObjectFactory::FInstantiateResult UArcMassEntityRootFactory::InstantiateReplicatedObjectFromHeader(
	const FInstantiateContext& Context,
	const UE::Net::FNetObjectCreationHeader* Header)
{
	FInstantiateResult Result;

	if (Header == nullptr)
	{
		Result.FailureDiagnosticMessage = TEXT("Header was null");
		return Result;
	}

	const uint32 ProtocolId = Header->GetProtocolId();

	UWorld* InstanceWorld = ArcMassEntityRootFactory_Private::FindWorldForReplicationSystem(Context.ResolveContext);
	if (InstanceWorld == nullptr)
	{
		Result.FailureDiagnosticMessage = TEXT("World for ReplicationSystem not resolvable");
		return Result;
	}
	UE_LOG(LogTemp, Log, TEXT("ArcMassEntityRootFactory: resolved instance world '%s' for ProtocolId=0x%x"),
		*InstanceWorld->GetName(), ProtocolId);

	UArcMassEntityReplicationSubsystem* Subsystem = InstanceWorld->GetSubsystem<UArcMassEntityReplicationSubsystem>();
	if (Subsystem == nullptr)
	{
		Result.FailureDiagnosticMessage = TEXT("ArcMassEntityReplicationSubsystem not available on resolved world");
		return Result;
	}

	UMassEntityConfigAsset* Config = Subsystem->FindConfigForProtocol(ProtocolId);
	if (Config == nullptr)
	{
		// Client-side stopgap: server-only StartReplicatingRootObject means the client
		// never populated ProtocolToConfig at warmup time. Fall back to the pending
		// client configs.
		Config = Subsystem->ResolveClientConfigForProtocol(ProtocolId);
	}
	if (Config == nullptr)
	{
		Result.FailureDiagnosticMessage = FString::Printf(
			TEXT("No config registered for protocol id 0x%x"), ProtocolId);
		return Result;
	}
	UE_LOG(LogTemp, Log, TEXT("ArcMassEntityRootFactory: instantiating from header (ProtocolId=0x%x, Config=%s)"),
		ProtocolId, *Config->GetName());

	UArcMassEntityVessel* Vessel = Subsystem->AcquireClientVessel(Config);
	if (Vessel == nullptr)
	{
		Result.FailureDiagnosticMessage = TEXT("Vessel pool exhausted and growth failed");
		return Result;
	}

	// Spawn the local Mass entity so the vessel has something to back its replicated state.
	// IMPORTANT: must use the world resolved from the bridge (InstanceWorld), not a global
	// best-effort lookup — otherwise in PIE we land the local entity in the server world
	// and the start observer fires reentrantly.
	const FMassEntityHandle SpawnedHandle = ArcMassEntityRootFactory_Private::SpawnLocalEntity(InstanceWorld, Config);
	if (!SpawnedHandle.IsValid())
	{
		Subsystem->ReleaseClientVessel(Vessel);
		Result.FailureDiagnosticMessage = FString::Printf(
			TEXT("Failed to spawn local Mass entity for config %s"), *Config->GetName());
		return Result;
	}
	Vessel->EntityHandle = SpawnedHandle;

	Result.Instance = Vessel;
	return Result;
}

void UArcMassEntityRootFactory::DetachedFromReplication(
	const FDetachContext& Context,
	const TOptional<FSubObjectDetachContext>& SubObjectContext)
{
	UArcMassEntityVessel* Vessel = Cast<UArcMassEntityVessel>(Context.DetachedInstance);
	if (Vessel == nullptr)
	{
		return;
	}

	UArcMassEntityReplicationSubsystem* Subsystem =
		ArcMassEntityRootFactory_Private::GetSubsystemFromObject(Vessel);
	if (Subsystem == nullptr)
	{
		return;
	}

	UWorld* World = Vessel->GetWorld();
	if (World == nullptr)
	{
		World = ArcMassEntityRootFactory_Private::FindGameWorld();
	}
	ArcMassEntityRootFactory_Private::DestroyLocalEntity(World, Vessel->EntityHandle);
	Subsystem->ReleaseClientVessel(Vessel);
}
