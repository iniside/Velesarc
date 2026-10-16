// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassRepE2EReplicationTests.h"

#include "ArcMassRepE2ESmokeTests.h"
#include "E2E/MTFEnv.h"
#include "E2E/MTFOpMacros.h"
#include "MassEntityConfigAsset.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityTemplate.h"
#include "Mass/EntityView.h"
#include "MassSpawnerSubsystem.h"
#include "Replication/ArcMassEntityVessel.h"
#include "Subsystem/ArcMassEntityReplicationSubsystem.h"
#include "UObject/UObjectIterator.h"

namespace ArcMassRepE2EReplication_Private
{
	const TCHAR* FixtureConfigPath = TEXT("/ArcMassReplicationTest/Fixtures/MEC_SmokeTestEntity.MEC_SmokeTestEntity");
	const TCHAR* PayloadVesselClassPath = TEXT("/Script/ArcMassReplicationTest.ArcMassRepProxy_TestPayload");
	const TCHAR* PayloadFragmentStructPath = TEXT("/Script/ArcMassReplicationTest.ArcMassTestPayloadFragment");

	UMassEntityConfigAsset* LoadFixtureConfig()
	{
		return LoadObject<UMassEntityConfigAsset>(nullptr, FixtureConfigPath);
	}

	UClass* LoadPayloadVesselClass()
	{
		return StaticLoadClass(UArcMassEntityVessel::StaticClass(), nullptr, PayloadVesselClassPath);
	}

	// Register the typed payload vessel for the fixture config on whichever side
	// this runs. Shared by the server spawn op and the client prepare op.
	bool RegisterPayloadVesselForFixture(UWorld* World, UMassEntityConfigAsset*& OutConfig)
	{
		OutConfig = LoadFixtureConfig();
		if (OutConfig == nullptr)
		{
			UE_LOGF(LogTemp, Error,"ArcMassRepE2E: fixture config not loadable at '%ls'", FixtureConfigPath);
			return false;
		}

		UClass* VesselClass = LoadPayloadVesselClass();
		if (VesselClass == nullptr)
		{
			UE_LOGF(LogTemp, Error,"ArcMassRepE2E: payload vessel class not found at '%ls'", PayloadVesselClassPath);
			return false;
		}

		UArcMassEntityReplicationSubsystem* Subsystem =
			World != nullptr ? World->GetSubsystem<UArcMassEntityReplicationSubsystem>() : nullptr;
		if (Subsystem == nullptr)
		{
			UE_LOGF(LogTemp, Error,"ArcMassRepE2E: no replication subsystem on world %ls", *GetNameSafe(World));
			return false;
		}

		Subsystem->RegisterVesselClassForConfig(OutConfig, VesselClass);
		return true;
	}

	// Read FArcMassTestPayloadFragment::Payload from a fragment instance by
	// reflection (the struct lives in the unexported CQTest module).
	bool ReadPayloadValue(const UScriptStruct* PayloadStruct, const void* FragmentMemory, uint32& OutValue)
	{
		const FProperty* PayloadProp = PayloadStruct->FindPropertyByName(TEXT("Payload"));
		if (PayloadProp == nullptr || FragmentMemory == nullptr)
		{
			return false;
		}
		OutValue = *PayloadProp->ContainerPtrToValuePtr<uint32>(FragmentMemory);
		return true;
	}
}

MTF_DEFINE_SERVER_OPERATION(UArcMassRepE2E_ServerOps, bool, SpawnPayloadEntity, uint32, Sentinel)

bool UArcMassRepE2E_ServerOps::SpawnPayloadEntity_Impl(uint32 Sentinel)
{
	UWorld* World = GetHandlerWorld();

	UMassEntityConfigAsset* Config = nullptr;
	if (!ArcMassRepE2EReplication_Private::RegisterPayloadVesselForFixture(World, Config))
	{
		return false;
	}

	UMassSpawnerSubsystem* Spawner = World->GetSubsystem<UMassSpawnerSubsystem>();
	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (Spawner == nullptr || EntitySubsystem == nullptr)
	{
		UE_LOGF(LogTemp, Error,"ArcMassRepE2E: Mass subsystems missing on server world");
		return false;
	}

	const FMassEntityTemplate& Template = Config->GetOrCreateEntityTemplate(*World);

	TArray<FMassEntityHandle> Spawned;
	TSharedPtr<FMassEntityManager::FEntityCreationContext> CreationContext =
		Spawner->SpawnEntities(Template, 1, Spawned);
	// Finalize spawning (runs observers, incl. the replication start observer).
	CreationContext.Reset();

	if (Spawned.Num() != 1)
	{
		UE_LOGF(LogTemp, Error,"ArcMassRepE2E: expected 1 spawned entity, got %d", Spawned.Num());
		return false;
	}

	const UScriptStruct* PayloadStruct = FindObject<UScriptStruct>(
		nullptr, ArcMassRepE2EReplication_Private::PayloadFragmentStructPath);
	if (PayloadStruct == nullptr)
	{
		UE_LOGF(LogTemp, Error,"ArcMassRepE2E: payload fragment struct not found");
		return false;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	const FMassEntityView EntityView(EntityManager, Spawned[0]);
	FStructView Fragment = EntityView.GetFragmentDataStruct(PayloadStruct);
	if (!Fragment.IsValid())
	{
		UE_LOGF(LogTemp, Error,"ArcMassRepE2E: spawned entity has no payload fragment");
		return false;
	}

	const FProperty* PayloadProp = PayloadStruct->FindPropertyByName(TEXT("Payload"));
	if (PayloadProp == nullptr)
	{
		return false;
	}
	*PayloadProp->ContainerPtrToValuePtr<uint32>(Fragment.GetMemory()) = Sentinel;

	UE_LOGF(LogTemp, Log,"ArcMassRepE2E: server spawned entity Index=%d and wrote sentinel %u",
		Spawned[0].Index, Sentinel);
	return true;
}

MTF_DEFINE_CLIENT_OPERATION(UArcMassRepE2E_ClientOps, bool, PrepareClient)

bool UArcMassRepE2E_ClientOps::PrepareClient_Impl()
{
	UMassEntityConfigAsset* Config = nullptr;
	return ArcMassRepE2EReplication_Private::RegisterPayloadVesselForFixture(GetHandlerWorld(), Config);
}

MTF_DEFINE_CLIENT_OPERATION(UArcMassRepE2E_ClientOps, int32, CountPayloadEntities, uint32, Sentinel)

int32 UArcMassRepE2E_ClientOps::CountPayloadEntities_Impl(uint32 Sentinel)
{
	UWorld* World = GetHandlerWorld();
	UMassEntitySubsystem* EntitySubsystem = World != nullptr ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (EntitySubsystem == nullptr)
	{
		return 0;
	}

	const UScriptStruct* PayloadStruct = FindObject<UScriptStruct>(
		nullptr, ArcMassRepE2EReplication_Private::PayloadFragmentStructPath);
	UClass* VesselClass = ArcMassRepE2EReplication_Private::LoadPayloadVesselClass();
	if (PayloadStruct == nullptr || VesselClass == nullptr)
	{
		return 0;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	// Walk the client-side vessels for this world and read the payload fragment
	// off each vessel's LOCAL Mass entity — a hit proves the wire value landed
	// in client fragment memory via OnRep -> ApplyReplicatedStateToEntity.
	int32 Count = 0;
	for (TObjectIterator<UArcMassEntityVessel> It; It; ++It)
	{
		UArcMassEntityVessel* Vessel = *It;
		if (!IsValid(Vessel) || !Vessel->IsA(VesselClass) || Vessel->GetWorld() != World)
		{
			continue;
		}
		if (!Vessel->EntityHandle.IsSet() || !EntityManager.IsEntityValid(Vessel->EntityHandle))
		{
			continue;
		}

		const FMassEntityView EntityView(EntityManager, Vessel->EntityHandle);
		const FStructView Fragment = EntityView.GetFragmentDataStruct(PayloadStruct);
		if (!Fragment.IsValid())
		{
			continue;
		}

		uint32 Value = 0;
		if (ArcMassRepE2EReplication_Private::ReadPayloadValue(PayloadStruct, Fragment.GetMemory(), Value)
			&& Value == Sentinel)
		{
			++Count;
		}
	}
	return Count;
}

TSubclassOf<UModularTestSuite> UArcMassRepE2E_EntityReplication::GetTestSuite_Implementation() const
{
	return UArcMassRepE2E_Suite::StaticClass();
}

FString UArcMassRepE2E_EntityReplication::GetTags_Implementation() const
{
	return TEXT("[Smoke]");
}

void UArcMassRepE2E_EntityReplication::RunTest_E2E_Implementation(UMTFEnv* Env, UMTFServerRole* Server, UMTFClientRole* Client)
{
	constexpr uint32 SentinelValue = 42u;

	UArcMassRepE2E_ServerOps* ServerOps = RequireServerOps<UArcMassRepE2E_ServerOps>(Server);
	UArcMassRepE2E_ClientOps* ClientOps = RequireClientOps<UArcMassRepE2E_ClientOps>(Client);
	if (HasFatalFailure())
	{
		return;
	}

	const bool bConnected = Env->TickUntilRolesReady();
	TEST_REQUIRE_TRUE(TEXT("Server ready and client connected"), bConnected);

	// Client must know the typed vessel class BEFORE the entity replicates, or
	// the factory instantiates a base identity-only vessel.
	TEST_REQUIRE_TRUE(TEXT("Client registered payload vessel for fixture"), ClientOps->PrepareClient());

	TEST_REQUIRE_TRUE(TEXT("Server spawned payload entity"), ServerOps->SpawnPayloadEntity(SentinelValue));

	const bool bReplicated = Env->TickUntil(TEXT("payload fragment replicated to client entity"),
		[ClientOps, SentinelValue]() { return ClientOps->CountPayloadEntities(SentinelValue) >= 1; });
	TEST_REQUIRE_TRUE(TEXT("Payload replicated to client-side Mass entity"), bReplicated);

	TEST_ASSERT_EQUAL_INT(TEXT("Exactly one client entity carries the sentinel"),
		ClientOps->CountPayloadEntities(SentinelValue), 1);
}
