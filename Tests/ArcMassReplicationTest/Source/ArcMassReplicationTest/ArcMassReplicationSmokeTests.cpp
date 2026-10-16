// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#include "GameFramework/GameModeBase.h"

#if ENABLE_PIE_NETWORK_TEST

#include "ArcMassRepProxy_TestPayload.h"
#include "ArcMassReplicationTestSettings.h"
#include "ArcMassTestPayloadFragment.h"
#include "Engine/World.h"
#include "Fragments/ArcMassReplicatedTag.h"
#include "Fragments/ArcMassReplicationFilter.h"
#include "MassEntityConfigAsset.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityQuery.h"
#include "MassEntitySubsystem.h"
#include "MassEntityTemplate.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "MassSpawnerSubsystem.h"
#include "Subsystem/ArcMassEntityReplicationSubsystem.h"
#include "Tests/AutomationCommon.h"

namespace ArcMassReplicationSmokeTest_Private
{
	// On-disk fixture config (UArcMassTestPayloadReplicationTrait pre-configured on the
	// asset). Both server and client load the SAME stably-named asset, so the creation
	// header's FNetObjectReference resolves client-side to the identical config and the
	// client's vessel-class map (keyed on the config instance) hits its own loaded copy.
	UMassEntityConfigAsset* LoadSmokeTestConfig()
	{
		const FSoftObjectPath& ConfigPath = GetDefault<UArcMassReplicationTestSettings>()->SmokeTestEntityConfig;
		return Cast<UMassEntityConfigAsset>(ConfigPath.TryLoad());
	}

	int32 CountEntitiesWithPayloadOnClient(UWorld* World, uint32 ExpectedPayload, uint32& OutFirstPayload)
	{
		OutFirstPayload = 0;
		if (World == nullptr)
		{
			return 0;
		}
		UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
		if (EntitySubsystem == nullptr)
		{
			return 0;
		}
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		const TSharedRef<FMassEntityManager> SharedManager = EntityManager.AsShared();

		FMassEntityQuery Query(SharedManager);
		Query.AddRequirement<FArcMassTestPayloadFragment>(EMassFragmentAccess::ReadOnly);
		Query.AddTagRequirement<FArcMassEntityReplicatedTag>(EMassFragmentPresence::All);

		int32 Count = 0;
		uint32 FoundPayload = 0;
		FMassExecutionContext ExecutionContext(EntityManager, /*InDeltaTimeSeconds=*/0.f);
		Query.ForEachEntityChunk(ExecutionContext, [&Count, &FoundPayload, ExpectedPayload](FMassExecutionContext& Context)
		{
			TConstArrayView<FArcMassTestPayloadFragment> Payloads = Context.GetFragmentView<FArcMassTestPayloadFragment>();
			for (int32 Index = 0; Index < Payloads.Num(); ++Index)
			{
				if (Count == 0)
				{
					FoundPayload = Payloads[Index].Payload;
				}
				++Count;
			}
		});
		OutFirstPayload = FoundPayload;
		return Count;
	}
}

NETWORK_TEST_CLASS(ArcMassReplicationSmokeTests, "Arc.MassReplication.Smoke")
{
	struct DerivedState : public FBasePIENetworkComponentState
	{
		// Raw pointer is GC-safe: RegisterVesselClassForConfig roots the config in the
		// subsystem's UPROPERTY ConfigToVesselClass map for the world's lifetime.
		UMassEntityConfigAsset* Config = nullptr;
		FMassEntityHandle ServerSpawnedEntity;
	};

	FPIENetworkComponent<DerivedState> Network{ TestRunner, TestCommandBuilder, bInitializing };
	inline static TSharedPtr<FScopedTestEnvironment> TestEnvironment{ nullptr };
	
	BEFORE_ALL()
	{
		TestEnvironment = FScopedTestEnvironment::Get();
		TestEnvironment->SetConsoleVariableValue(TEXT("net.Iris.UseIrisReplication"), TEXT("1"));
	}
	
	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;

		// Mass logs an Error when its observer/system queries are cached during PIE
		// world init with no entities yet to validate against — happens before our
		// test body runs. SendRPC warnings come from PlayerController teardown when
		// the host closes the connection. Both are noise relative to what we test.
		TestRunner->AddExpectedError(TEXT("FMassEntityQuery::CacheArchetypes"), EAutomationExpectedErrorFlags::Contains, -1);
		TestRunner->AddExpectedError(TEXT("SendRPC"), EAutomationExpectedErrorFlags::Contains, -1);

		// Pre-existing project-content errors that fire during PIE world load:
		// ArcRPG StateTree compile failures and an AnimGraph CopyMotion struct missing.
		// Unrelated to ArcMassReplication; tracked in their own backlogs.
		TestRunner->AddExpectedError(TEXT("Failed to compile"), EAutomationExpectedErrorFlags::Contains, -1);
		TestRunner->AddExpectedError(TEXT("AnimGraphNode_CopyMotion"), EAutomationExpectedErrorFlags::Contains, -1);
		TestRunner->AddExpectedError(TEXT("Failed to create state utility considerations"), EAutomationExpectedErrorFlags::Contains, -1);
		TestRunner->AddExpectedError(TEXT("Failed to create condition for transition"), EAutomationExpectedErrorFlags::Contains, -1);
		TestRunner->AddExpectedError(TEXT("Cannot copy properties between Parameter"), EAutomationExpectedErrorFlags::Contains, -1);
		TestRunner->AddExpectedError(TEXT("Malformed target property path"), EAutomationExpectedErrorFlags::Contains, -1);
		TestRunner->AddExpectedError(TEXT("On Delegate Transition to"), EAutomationExpectedErrorFlags::Contains, -1);

		FNetworkComponentBuilder<DerivedState>()
			.WithClients(1)
			.AsDedicatedServer()
			.WithGameInstanceClass(UGameInstance::StaticClass())
			.WithGameMode(AGameModeBase::StaticClass())
			.Build(Network);
	}

	AFTER_ALL()
	{
		TestEnvironment = nullptr;
	}
	
	TEST_METHOD(ServerSpawnsTaggedEntity_ClientReceivesFragmentData)
	{
		constexpr uint32 SentinelValue = 42u;

		Network
		.ThenServer(TEXT("Load fixture config on server"), [this](DerivedState& ServerState)
		{
			UE_LOGF(LogTemp, Log,"ArcMassReplicationSmoke: STEP 1 — Load fixture config on server (entered)");

			ServerState.Config = ArcMassReplicationSmokeTest_Private::LoadSmokeTestConfig();
			ASSERT_THAT(IsNotNull(ServerState.Config));
			UE_LOGF(LogTemp, Log,"ArcMassReplicationSmoke: STEP 1 — loaded config '%ls'", *ServerState.Config->GetName());

			UArcMassEntityReplicationSubsystem* Subsystem =
				ServerState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
			ASSERT_THAT(IsNotNull(Subsystem));
			UE_LOGF(LogTemp, Log,"ArcMassReplicationSmoke: STEP 1 — got server subsystem");

			Subsystem->RegisterVesselClassForConfig(
				ServerState.Config, UArcMassRepProxy_TestPayload::StaticClass());
			UE_LOGF(LogTemp, Log,"ArcMassReplicationSmoke: STEP 1 — registered vessel class UArcMassRepProxy_TestPayload");
			UE_LOGF(LogTemp, Log,"ArcMassReplicationSmoke: STEP 1 — done");
		})
		.ThenClients(TEXT("Load fixture config on client"), [this](DerivedState& ClientState)
		{
			UE_LOGF(LogTemp, Log,"ArcMassReplicationSmoke: STEP 2 — Load fixture config on client (entered)");

			ClientState.Config = ArcMassReplicationSmokeTest_Private::LoadSmokeTestConfig();
			ASSERT_THAT(IsNotNull(ClientState.Config));
			UE_LOGF(LogTemp, Log,"ArcMassReplicationSmoke: STEP 2 — loaded config '%ls'", *ClientState.Config->GetName());

			UArcMassEntityReplicationSubsystem* Subsystem =
				ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
			ASSERT_THAT(IsNotNull(Subsystem));
			UE_LOGF(LogTemp, Log,"ArcMassReplicationSmoke: STEP 2 — got client subsystem");

			Subsystem->RegisterVesselClassForConfig(
				ClientState.Config, UArcMassRepProxy_TestPayload::StaticClass());
			UE_LOGF(LogTemp, Log,"ArcMassReplicationSmoke: STEP 2 — registered vessel class UArcMassRepProxy_TestPayload");
			UE_LOGF(LogTemp, Log,"ArcMassReplicationSmoke: STEP 2 — done");
		})
		.ThenServer(TEXT("Spawn tagged Mass entity on server"), [this, SentinelValue](DerivedState& ServerState)
		{
			UE_LOGF(LogTemp, Log,"ArcMassReplicationSmoke: STEP 3 — Spawn tagged Mass entity on server (entered)");

			ASSERT_THAT(IsNotNull(ServerState.Config));
			ASSERT_THAT(IsNotNull(ServerState.World));

			UMassSpawnerSubsystem* Spawner = ServerState.World->GetSubsystem<UMassSpawnerSubsystem>();
			ASSERT_THAT(IsNotNull(Spawner));
			UE_LOGF(LogTemp, Log,"ArcMassReplicationSmoke: STEP 3 — got spawner");

			const FMassEntityTemplate& Template = ServerState.Config->GetOrCreateEntityTemplate(*ServerState.World);
			UE_LOGF(LogTemp, Log,"ArcMassReplicationSmoke: STEP 3 — built/cached template");

			TArray<FMassEntityHandle> Spawned;
			TSharedPtr<FMassEntityManager::FEntityCreationContext> CreationContext =
				Spawner->SpawnEntities(Template, 1, Spawned);
			CreationContext.Reset();
			UE_LOGF(LogTemp, Log,"ArcMassReplicationSmoke: STEP 3 — SpawnEntities returned %d handles", Spawned.Num());

			ASSERT_THAT(AreEqual(Spawned.Num(), 1));
			ServerState.ServerSpawnedEntity = Spawned[0];
			UE_LOGF(LogTemp, Log,"ArcMassReplicationSmoke: STEP 3 — entity handle Index=%d Serial=%d",
				ServerState.ServerSpawnedEntity.Index, ServerState.ServerSpawnedEntity.SerialNumber);

			// Write the sentinel into the entity's payload fragment so we can verify
			// it propagates to the client.
			UMassEntitySubsystem* EntitySubsystem = ServerState.World->GetSubsystem<UMassEntitySubsystem>();
			ASSERT_THAT(IsNotNull(EntitySubsystem));
			FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

			FArcMassTestPayloadFragment* Payload =
				EntityManager.GetFragmentDataPtr<FArcMassTestPayloadFragment>(ServerState.ServerSpawnedEntity);
			ASSERT_THAT(IsNotNull(Payload));
			Payload->Payload = SentinelValue;
			UE_LOGF(LogTemp, Log,"ArcMassReplicationSmoke: STEP 3 — wrote sentinel %u into payload, done", SentinelValue);
		})
		.UntilClients(TEXT("Wait for replicated entity on client"), [SentinelValue](DerivedState& ClientState) -> bool
		{
			uint32 FoundPayload = 0;
			const int32 Count = ArcMassReplicationSmokeTest_Private::CountEntitiesWithPayloadOnClient(
				ClientState.World, SentinelValue, FoundPayload);
			const bool bReady = Count >= 1 && FoundPayload == SentinelValue;
			UE_LOGF(LogTemp, Verbose,"ArcMassReplicationSmoke: STEP 4 — Until poll: Count=%d FoundPayload=%u Ready=%ls",
				Count, FoundPayload, bReady ? TEXT("true") : TEXT("false"));
			return bReady;
		}, FTimespan::FromSeconds(15))
		.ThenClients(TEXT("Assert client received the sentinel payload"), [this, SentinelValue](DerivedState& ClientState)
		{
			UE_LOGF(LogTemp, Log,"ArcMassReplicationSmoke: STEP 5 — Assert client received the sentinel payload (entered)");

			uint32 FoundPayload = 0;
			const int32 Count = ArcMassReplicationSmokeTest_Private::CountEntitiesWithPayloadOnClient(
				ClientState.World, SentinelValue, FoundPayload);
			UE_LOGF(LogTemp, Log,"ArcMassReplicationSmoke: STEP 5 — final client query: Count=%d FoundPayload=%u (Expected=%u)",
				Count, FoundPayload, SentinelValue);
			ASSERT_THAT(AreEqual(Count, 1));
			ASSERT_THAT(AreEqual(FoundPayload, SentinelValue));
			UE_LOGF(LogTemp, Log,"ArcMassReplicationSmoke: STEP 5 — done");
		});
	}
};

#endif // ENABLE_PIE_NETWORK_TEST
