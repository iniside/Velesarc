// Copyright Lukasz Baran. All Rights Reserved.

#include "Replication/ArcMassEntityRootFactory.h"

#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "Iris/ReplicationSystem/ObjectReferenceCacheFwd.h"
#include "Iris/ReplicationSystem/ObjectReplicationBridge.h"
#include "Iris/ReplicationSystem/ReplicationBridgeTypes.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/NetSerializationContext.h"
#include "Iris/Serialization/ObjectNetSerializer.h"
#include "MassEntityConfigAsset.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityTemplate.h"
#include "MassSpawnerSubsystem.h"
#include "Replication/ArcMassEntityVessel.h"
#include "Replication/ArcMassIrisWorldResolver.h"
#include "Subsystem/ArcMassEntityReplicationSubsystem.h"

namespace ArcMassEntityRootFactory_Private
{
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
			UE_LOGF(LogTemp, Warning,"ArcMassReplication: SpawnLocalEntity got 0 entities for config %ls",
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
}

FString FArcMassNetCreationHeader::ToString() const
{
	return FString::Printf(
		TEXT("FArcMassNetCreationHeader (ProtocolId:0x%x): ProtocolVersion=%u ConfigReference=%s"),
		GetProtocolId(), ProtocolVersion, *ConfigReference.ToString());
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

TUniquePtr<UE::Net::FNetObjectCreationHeader> UArcMassEntityRootFactory::CreateAndFillHeader(
	UE::Net::FNetRefHandle Handle)
{
	if (Bridge == nullptr)
	{
		UE_LOGF(LogTemp, Error,"ArcMassEntityRootFactory: CreateAndFillHeader has null Bridge for handle %ls",
			*Handle.ToString());
		return nullptr;
	}

	UObject* Instance = Bridge->GetReplicatedObject(Handle);
	UArcMassEntityVessel* Vessel = Cast<UArcMassEntityVessel>(Instance);
	if (Vessel == nullptr)
	{
		UE_LOGF(LogTemp, Error,"ArcMassEntityRootFactory: CreateAndFillHeader found no vessel for handle %ls (instance %ls)",
			*Handle.ToString(), *GetNameSafe(Instance));
		return nullptr;
	}
	if (Vessel->Config == nullptr)
	{
		UE_LOGF(LogTemp, Error,"ArcMassEntityRootFactory: CreateAndFillHeader vessel %ls has no Config (handle %ls)",
			*GetNameSafe(Vessel), *Handle.ToString());
		return nullptr;
	}

	TUniquePtr<FArcMassNetCreationHeader> Header = MakeUnique<FArcMassNetCreationHeader>();
	Header->ConfigReference = Bridge->GetOrCreateObjectReference(Vessel->Config);
	// GetOrCreateObjectReference fails SILENTLY (returns an invalid reference) for
	// non-stably-named objects, e.g. a transient in-memory config. Rejecting here keeps
	// the failure server-side; letting it through would surface as a connection-level
	// read error on the client (creation headers have no pending-resolve/retry path).
	if (!Header->ConfigReference.IsValid())
	{
		UE_LOGF(LogTemp, Error,
			"ArcMassEntityRootFactory: config %ls is not referencable over the network "
			"(not stably named?) — refusing to replicate vessel %ls (handle %ls)",
			*GetNameSafe(Vessel->Config), *GetNameSafe(Vessel), *Handle.ToString());
		return nullptr;
	}

	return Header;
}

bool UArcMassEntityRootFactory::SerializeHeader(
	const UE::Net::FCreationHeaderContext& Context,
	const UE::Net::FNetObjectCreationHeader* Header)
{
	const FArcMassNetCreationHeader* ArcHeader = static_cast<const FArcMassNetCreationHeader*>(Header);

	UE::Net::FNetBitStreamWriter* Writer = Context.Serialization.GetBitStreamWriter();

	// ProtocolVersion goes first on the wire so a mismatched receiver rejects the
	// header before reading (or allocating) anything else.
	Writer->WriteBits(ArcHeader->ProtocolVersion, 32);
	UE::Net::WriteFullNetObjectReference(Context.Serialization, ArcHeader->ConfigReference);

	return !Writer->IsOverflown();
}

TUniquePtr<UE::Net::FNetObjectCreationHeader> UArcMassEntityRootFactory::CreateAndDeserializeHeader(
	const UE::Net::FCreationHeaderContext& Context)
{
	UE::Net::FNetBitStreamReader* Reader = Context.Serialization.GetBitStreamReader();

	// Version check runs before any allocation: a mismatch means the wire format
	// itself differs between server and client builds.
	const uint32 ReceivedProtocolVersion = Reader->ReadBits(32);
	if (ReceivedProtocolVersion != ArcMassNetProtocolVersion)
	{
		UE_LOGF(LogTemp, Warning,
			"ArcMassEntityRootFactory: header rejected — ProtocolVersion mismatch (received=%u expected=%u). "
			"Server and client must run the same ArcMassReplication wire-format version.",
			ReceivedProtocolVersion, ArcMassNetProtocolVersion);
		return nullptr;
	}

	TUniquePtr<FArcMassNetCreationHeader> Header = MakeUnique<FArcMassNetCreationHeader>();
	Header->ProtocolVersion = ReceivedProtocolVersion;
	UE::Net::ReadFullNetObjectReference(Context.Serialization, Header->ConfigReference);

	if (Reader->IsOverflown())
	{
		return nullptr;
	}

	return Header;
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

	const FArcMassNetCreationHeader* ArcHeader = static_cast<const FArcMassNetCreationHeader*>(Header);

	UWorld* InstanceWorld = ArcMassIrisWorldResolver::FindWorldForReplicationSystem(Context.ResolveContext);
	if (InstanceWorld == nullptr)
	{
		Result.FailureDiagnosticMessage = TEXT("World for ReplicationSystem not resolvable");
		return Result;
	}
	UE_LOGF(LogTemp, Log,"ArcMassEntityRootFactory: resolved instance world '%ls' for header %ls",
		*InstanceWorld->GetName(), *ArcHeader->ToString());

	UArcMassEntityReplicationSubsystem* Subsystem = InstanceWorld->GetSubsystem<UArcMassEntityReplicationSubsystem>();
	if (Subsystem == nullptr)
	{
		Result.FailureDiagnosticMessage = TEXT("ArcMassEntityReplicationSubsystem not available on resolved world");
		return Result;
	}

	if (Bridge == nullptr)
	{
		Result.FailureDiagnosticMessage = TEXT("Null Bridge — cannot resolve config reference");
		return Result;
	}

	// Resolve the config the server referenced in the creation header. First arrival of
	// an unloaded config goes through the reference cache's blocking StaticLoadObject
	// last resort — a sync-load hitch on the receive path, accepted until the codegen
	// startup module preloads production configs.
	UObject* ConfigObject = Bridge->ResolveObjectReference(ArcHeader->ConfigReference, Context.ResolveContext);
	UMassEntityConfigAsset* Config = Cast<UMassEntityConfigAsset>(ConfigObject);
	if (Config == nullptr)
	{
		Result.FailureDiagnosticMessage = FString::Printf(
			TEXT("Config reference %s unresolvable on client (resolved object: %s)"),
			*ArcHeader->ConfigReference.ToString(), *GetNameSafe(ConfigObject));
		return Result;
	}
	UE_LOGF(LogTemp, Log,"ArcMassEntityRootFactory: instantiating from header (Config=%ls)",
		*Config->GetName());

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
	// Client vessels are unpooled and Iris-owned: let the remote peer's destroy
	// request destroy the instance (DetachedFromReplication then releases it).
	Result.Flags |= EReplicationBridgeCreateNetRefHandleResultFlags::AllowDestroyInstanceFromRemote;
	return Result;
}

void UArcMassEntityRootFactory::DetachedFromReplication(
	const FDetachContext& Context,
	const TOptional<FSubObjectDetachContext>& SubObjectContext)
{
	// DetachedInstance is documented nullable (the instance may already be gone).
	if (Context.DetachedInstance == nullptr)
	{
		return;
	}

	UArcMassEntityVessel* Vessel = Cast<UArcMassEntityVessel>(Context.DetachedInstance);
	if (Vessel == nullptr)
	{
		return;
	}

	// Vessel outer = subsystem -> world, so GetWorld() is valid for a live vessel.
	UWorld* World = Vessel->GetWorld();
	if (World == nullptr)
	{
		return;
	}

	UArcMassEntityReplicationSubsystem* Subsystem = World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
	if (Subsystem == nullptr)
	{
		return;
	}

	Subsystem->DestroyLocalEntityForVessel(Vessel);
	Subsystem->ReleaseClientVessel(Vessel);
}
