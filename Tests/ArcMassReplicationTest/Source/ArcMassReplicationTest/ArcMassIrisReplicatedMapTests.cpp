// Copyright Lukasz Baran. All Rights Reserved.

// PIE end-to-end tests for FArcIrisReplicatedMap / FArcIrisReplicatedMapNetSerializer.
//
// Architecture note: to exercise SerializeDelta/DeserializeDelta the map MUST be a DIRECT
// top-level replicated UPROPERTY on an actor (not nested inside FArcIrisEntityArray via the
// proxy SetEntityFragments path, which serializes through FStructNetSerializer and never
// calls the map's own SerializeDelta). Delta compression is enabled explicitly via
// UReplicationSystem::SetDeltaCompressionStatus after the actor is registered.
//
// Packet-loss correctness rests on the design-A snapshot model (SerializeDelta diffs vs
// Args.Prev -- the per-connection acked baseline), not simulated loss here.
// Late-join is exercised by populating the map before verifying clients -- FNetworkComponentBuilder
// does not expose a post-construction "add client" API, so late-join is tested as: server
// populates first, then UntilClients waits for convergence on the single client. The initial-state
// path (Args.Prev == nullptr -> every entry is "added") covers the same code as a real late-join.
//
// Substitution note: FArcItemId is in ArcMassItemsRuntime -- a module not already in
// ArcMassReplicationTest.Build.cs. FGameplayTag is used as the map key for the instanced-struct
// map to avoid adding that dependency. Build.cs was NOT edited.

#include "CQTest.h"
#include "CQTestSettings.h"
#include "Components/PIENetworkComponent.h"
#include "ArcMassTestItemFragment.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Tests/AutomationCommon.h"
#include "Net/Iris/ReplicationSystem/ReplicationSystemUtil.h"
#include "Net/Iris/ReplicationSystem/EngineReplicationBridge.h"
#include "Iris/ReplicationSystem/ObjectReplicationBridge.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/ReplicationSystemTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "GameplayTagsManager.h"

#if ENABLE_PIE_NETWORK_TEST && UE_WITH_IRIS

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace ArcMassMapTestPrivate
{
	// Enable Iris delta compression on an actor after it begins replicating.
	// Approach:
	//   1. UE::Net::FReplicationSystemUtil::GetActorReplicationBridge(Actor) -> UObjectReplicationBridge*
	//   2. Bridge->GetReplicatedRefHandle(Actor) -> FNetRefHandle
	//   3. RepSystem->SetDeltaCompressionStatus(Handle, Allow)
	// net.Iris.EnableDeltaCompression defaults to true (DeltaCompressionBaselineManager.cpp:29).
	// SerializeDelta fires iff DC is enabled on the owning object AND the global CVar is true.
	static void EnableDeltaCompression(AActor* Actor)
	{
		if (Actor == nullptr)
		{
			return;
		}
		UObjectReplicationBridge* Bridge = Cast<UObjectReplicationBridge>(UE::Net::FReplicationSystemUtil::GetActorReplicationBridge(Actor));
		if (Bridge == nullptr)
		{
			return;
		}
		UReplicationSystem* RepSystem = UE::Net::FReplicationSystemUtil::GetReplicationSystem(Actor);
		if (RepSystem == nullptr)
		{
			return;
		}
		UE::Net::FNetRefHandle RefHandle = Bridge->GetReplicatedRefHandle(Actor);
		if (!RefHandle.IsValid())
		{
			return;
		}
		RepSystem->SetDeltaCompressionStatus(RefHandle, UE::Net::ENetObjectDeltaCompressionStatus::Allow);
		UE_LOGF(LogTemp, Log,"ArcMassMapTest: delta compression enabled for %ls", *Actor->GetName());
	}

	// Create a native gameplay tag for test use. UGameplayTagsManager::AddNativeGameplayTag
	// registers the tag if not already present -- safe to call multiple times with the same name.
	static FGameplayTag MakeTestTag(const FName TagName)
	{
		return UGameplayTagsManager::Get().AddNativeGameplayTag(TagName);
	}
} // namespace ArcMassMapTestPrivate

// ---------------------------------------------------------------------------
// Shared fixture state
// ---------------------------------------------------------------------------

struct FDirectMapTestState : public FBasePIENetworkComponentState
{
	AArcMassTestDirectMapActor* Actor = nullptr;
};

// ---------------------------------------------------------------------------
// NETWORK_TEST_CLASS — ArcMassReplication.IrisMap (single-client tests)
// ---------------------------------------------------------------------------

NETWORK_TEST_CLASS(ArcIrisReplicatedMapTests, "ArcMassReplication.IrisMap")
{
	FPIENetworkComponent<FDirectMapTestState> Network{ TestRunner, TestCommandBuilder, bInitializing };

	inline static TSharedPtr<FScopedTestEnvironment> TestEnvironment{ nullptr };

	BEFORE_ALL()
	{
		TestEnvironment = FScopedTestEnvironment::Get();
		// Iris replication -- mirrors ArcIrisDirectArrayReplication.
		TestEnvironment->SetConsoleVariableValue(TEXT("net.Iris.UseIrisReplication"), TEXT("1"));
		// net.Iris.EnableDeltaCompression defaults to true; set explicitly so the test is
		// self-documenting. SerializeDelta fires only when this CVar is true AND
		// SetDeltaCompressionStatus(Allow) has been called on the actor's NetRefHandle.
		TestEnvironment->SetConsoleVariableValue(TEXT("net.Iris.EnableDeltaCompression"), TEXT("1"));
	}

	BEFORE_EACH()
	{
		TestRunner->AddExpectedError(TEXT("FMassEntityQuery::CacheArchetypes"), EAutomationExpectedErrorFlags::Contains, -1);
		TestRunner->AddExpectedError(TEXT("SendRPC"), EAutomationExpectedErrorFlags::Contains, -1);

		FNetworkComponentBuilder<FDirectMapTestState>()
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

	// -----------------------------------------------------------------------
	// Test 1: Single entry replicates to client
	// -----------------------------------------------------------------------
	TEST_METHOD(SingleEntry_ReplicatesToClient)
	{
		Network
			.ThenServer(TEXT("Spawn actor, add one int->struct entry"), [this](FDirectMapTestState& ServerState)
			{
				ServerState.Actor = ServerState.World->SpawnActor<AArcMassTestDirectMapActor>();
				ASSERT_THAT(IsNotNull(ServerState.Actor));
				ArcMassMapTestPrivate::EnableDeltaCompression(ServerState.Actor);

				FArcMassTestReplicatedItem Val;
				Val.ItemId = 1;
				Val.Value  = 100;
				ServerState.Actor->IntStructMap.Entries.Add(1, Val);

				UE_LOGF(LogTemp, Log,"IrisMap SingleEntry: server added Key=1 ItemId=1 Value=100");
			})
			.UntilClients(TEXT("Wait for map entry on client"), [](FDirectMapTestState& ClientState) -> bool
			{
				for (TActorIterator<AArcMassTestDirectMapActor> It(ClientState.World); It; ++It)
				{
					if (It->IntStructMap.Entries.Num() == 1)
					{
						ClientState.Actor = *It;
						return true;
					}
				}
				return false;
			})
			.ThenClients(TEXT("Verify single entry"), [this](FDirectMapTestState& ClientState)
			{
				ASSERT_THAT(IsNotNull(ClientState.Actor));
				ASSERT_THAT(AreEqual(ClientState.Actor->IntStructMap.Entries.Num(), 1));
				const FArcMassTestReplicatedItem* Val = ClientState.Actor->IntStructMap.Entries.Find(1);
				ASSERT_THAT(IsNotNull(Val));
				ASSERT_THAT(AreEqual(Val->ItemId, 1));
				ASSERT_THAT(AreEqual(Val->Value, 100));
				UE_LOGF(LogTemp, Log,"IrisMap SingleEntry: client verified Key=1 ItemId=1 Value=100");
			});
	}

	// -----------------------------------------------------------------------
	// Test 2: Multiple entries replicate to client
	// -----------------------------------------------------------------------
	TEST_METHOD(MultipleEntries_ReplicateToClient)
	{
		Network
			.ThenServer(TEXT("Spawn actor, add 3 int->struct entries"), [this](FDirectMapTestState& ServerState)
			{
				ServerState.Actor = ServerState.World->SpawnActor<AArcMassTestDirectMapActor>();
				ASSERT_THAT(IsNotNull(ServerState.Actor));
				ArcMassMapTestPrivate::EnableDeltaCompression(ServerState.Actor);

				FArcMassTestReplicatedItem A;
				A.ItemId = 10; A.Value = 100;
				ServerState.Actor->IntStructMap.Entries.Add(10, A);

				FArcMassTestReplicatedItem B;
				B.ItemId = 20; B.Value = 200;
				ServerState.Actor->IntStructMap.Entries.Add(20, B);

				FArcMassTestReplicatedItem C;
				C.ItemId = 30; C.Value = 300;
				ServerState.Actor->IntStructMap.Entries.Add(30, C);
			})
			.UntilClients(TEXT("Wait for 3 entries"), [](FDirectMapTestState& ClientState) -> bool
			{
				for (TActorIterator<AArcMassTestDirectMapActor> It(ClientState.World); It; ++It)
				{
					if (It->IntStructMap.Entries.Num() == 3)
					{
						ClientState.Actor = *It;
						return true;
					}
				}
				return false;
			})
			.ThenClients(TEXT("Verify 3 entries"), [this](FDirectMapTestState& ClientState)
			{
				ASSERT_THAT(IsNotNull(ClientState.Actor));
				ASSERT_THAT(AreEqual(ClientState.Actor->IntStructMap.Entries.Num(), 3));

				const FArcMassTestReplicatedItem* A = ClientState.Actor->IntStructMap.Entries.Find(10);
				ASSERT_THAT(IsNotNull(A));
				ASSERT_THAT(AreEqual(A->Value, 100));

				const FArcMassTestReplicatedItem* B = ClientState.Actor->IntStructMap.Entries.Find(20);
				ASSERT_THAT(IsNotNull(B));
				ASSERT_THAT(AreEqual(B->Value, 200));

				const FArcMassTestReplicatedItem* C = ClientState.Actor->IntStructMap.Entries.Find(30);
				ASSERT_THAT(IsNotNull(C));
				ASSERT_THAT(AreEqual(C->Value, 300));
			});
	}

	// -----------------------------------------------------------------------
	// Test 3: Change one value replicates to client (exercises SerializeDelta changed path)
	// -----------------------------------------------------------------------
	TEST_METHOD(ChangeOneValue_ReplicatesToClient)
	{
		Network
			.ThenServer(TEXT("Spawn actor, add 3 entries"), [this](FDirectMapTestState& ServerState)
			{
				ServerState.Actor = ServerState.World->SpawnActor<AArcMassTestDirectMapActor>();
				ASSERT_THAT(IsNotNull(ServerState.Actor));
				ArcMassMapTestPrivate::EnableDeltaCompression(ServerState.Actor);

				FArcMassTestReplicatedItem A;
				A.ItemId = 1; A.Value = 10;
				ServerState.Actor->IntStructMap.Entries.Add(1, A);

				FArcMassTestReplicatedItem B;
				B.ItemId = 2; B.Value = 20;
				ServerState.Actor->IntStructMap.Entries.Add(2, B);

				FArcMassTestReplicatedItem C;
				C.ItemId = 3; C.Value = 30;
				ServerState.Actor->IntStructMap.Entries.Add(3, C);
			})
			.UntilClients(TEXT("Wait for 3 entries (initial)"), [](FDirectMapTestState& ClientState) -> bool
			{
				for (TActorIterator<AArcMassTestDirectMapActor> It(ClientState.World); It; ++It)
				{
					if (It->IntStructMap.Entries.Num() == 3)
					{
						ClientState.Actor = *It;
						return true;
					}
				}
				return false;
			})
			.ThenServer(TEXT("Mutate Key=2 Value from 20 to 999"), [this](FDirectMapTestState& ServerState)
			{
				ASSERT_THAT(IsNotNull(ServerState.Actor));
				FArcMassTestReplicatedItem* Entry = ServerState.Actor->IntStructMap.Entries.Find(2);
				ASSERT_THAT(IsNotNull(Entry));
				Entry->Value = 999;
				// Iris polls UPROPERTY state -- no explicit MarkDirty needed for a plain struct UPROPERTY.
				// (FArcIrisReplicatedMap is NOT push-model; Iris diffs via Quantize snapshots.)
			})
			.UntilClients(TEXT("Wait for Key=2 update"), [](FDirectMapTestState& ClientState) -> bool
			{
				if (ClientState.Actor == nullptr)
				{
					for (TActorIterator<AArcMassTestDirectMapActor> It(ClientState.World); It; ++It)
					{
						ClientState.Actor = *It;
						break;
					}
				}
				if (ClientState.Actor == nullptr) return false;
				const FArcMassTestReplicatedItem* Entry = ClientState.Actor->IntStructMap.Entries.Find(2);
				return Entry != nullptr && Entry->Value == 999;
			})
			.ThenClients(TEXT("Verify Key=2 changed, others unchanged"), [this](FDirectMapTestState& ClientState)
			{
				ASSERT_THAT(IsNotNull(ClientState.Actor));
				ASSERT_THAT(AreEqual(ClientState.Actor->IntStructMap.Entries.Num(), 3));

				const FArcMassTestReplicatedItem* A = ClientState.Actor->IntStructMap.Entries.Find(1);
				ASSERT_THAT(IsNotNull(A));
				ASSERT_THAT(AreEqual(A->Value, 10));

				const FArcMassTestReplicatedItem* B = ClientState.Actor->IntStructMap.Entries.Find(2);
				ASSERT_THAT(IsNotNull(B));
				ASSERT_THAT(AreEqual(B->Value, 999));

				const FArcMassTestReplicatedItem* C = ClientState.Actor->IntStructMap.Entries.Find(3);
				ASSERT_THAT(IsNotNull(C));
				ASSERT_THAT(AreEqual(C->Value, 30));
			});
	}

	// -----------------------------------------------------------------------
	// Test 4: Remove one key replicates to client (exercises SerializeDelta removed path)
	// -----------------------------------------------------------------------
	TEST_METHOD(RemoveOneKey_ReplicatesToClient)
	{
		Network
			.ThenServer(TEXT("Spawn actor, add 4 entries"), [this](FDirectMapTestState& ServerState)
			{
				ServerState.Actor = ServerState.World->SpawnActor<AArcMassTestDirectMapActor>();
				ASSERT_THAT(IsNotNull(ServerState.Actor));
				ArcMassMapTestPrivate::EnableDeltaCompression(ServerState.Actor);

				FArcMassTestReplicatedItem V;
				for (int32 i = 1; i <= 4; ++i)
				{
					V.ItemId = i;
					V.Value  = i * 10;
					ServerState.Actor->IntStructMap.Entries.Add(i, V);
				}
			})
			.UntilClients(TEXT("Wait for 4 entries"), [](FDirectMapTestState& ClientState) -> bool
			{
				for (TActorIterator<AArcMassTestDirectMapActor> It(ClientState.World); It; ++It)
				{
					if (It->IntStructMap.Entries.Num() == 4)
					{
						ClientState.Actor = *It;
						return true;
					}
				}
				return false;
			})
			.ThenServer(TEXT("Remove Key=2"), [this](FDirectMapTestState& ServerState)
			{
				ASSERT_THAT(IsNotNull(ServerState.Actor));
				ServerState.Actor->IntStructMap.Entries.Remove(2);
			})
			.UntilClients(TEXT("Wait for 3 entries (after remove)"), [](FDirectMapTestState& ClientState) -> bool
			{
				if (ClientState.Actor == nullptr)
				{
					for (TActorIterator<AArcMassTestDirectMapActor> It(ClientState.World); It; ++It)
					{
						ClientState.Actor = *It;
						break;
					}
				}
				if (ClientState.Actor == nullptr) return false;
				return ClientState.Actor->IntStructMap.Entries.Num() == 3;
			})
			.ThenClients(TEXT("Verify Key=2 absent, others present"), [this](FDirectMapTestState& ClientState)
			{
				ASSERT_THAT(IsNotNull(ClientState.Actor));
				ASSERT_THAT(AreEqual(ClientState.Actor->IntStructMap.Entries.Num(), 3));
				ASSERT_THAT(IsTrue(ClientState.Actor->IntStructMap.Entries.Find(2) == nullptr));
				ASSERT_THAT(IsTrue(ClientState.Actor->IntStructMap.Entries.Contains(1)));
				ASSERT_THAT(IsTrue(ClientState.Actor->IntStructMap.Entries.Contains(3)));
				ASSERT_THAT(IsTrue(ClientState.Actor->IntStructMap.Entries.Contains(4)));
			});
	}

	// -----------------------------------------------------------------------
	// Test 5: FGameplayTag->float map replicates to client.
	// Tags are registered via AddNativeGameplayTag to avoid returning an invalid tag
	// (FGameplayTag::RequestGameplayTag with bErrorIfNotFound=false returns None for
	// unregistered tags, causing map key collisions if both tags resolve to None).
	// -----------------------------------------------------------------------
	TEST_METHOD(TagFloatMap_ReplicatesToClient)
	{
		Network
			.ThenServer(TEXT("Spawn actor, add 2 tag->float entries"), [this](FDirectMapTestState& ServerState)
			{
				ServerState.Actor = ServerState.World->SpawnActor<AArcMassTestDirectMapActor>();
				ASSERT_THAT(IsNotNull(ServerState.Actor));
				ArcMassMapTestPrivate::EnableDeltaCompression(ServerState.Actor);

				const FGameplayTag TagA = ArcMassMapTestPrivate::MakeTestTag(FName(TEXT("Test.Map.A")));
				const FGameplayTag TagB = ArcMassMapTestPrivate::MakeTestTag(FName(TEXT("Test.Map.B")));
				ASSERT_THAT(IsTrue(TagA.IsValid()));
				ASSERT_THAT(IsTrue(TagB.IsValid()));

				ServerState.Actor->TagFloatMap.Entries.Add(TagA, 1.5f);
				ServerState.Actor->TagFloatMap.Entries.Add(TagB, 2.5f);
			})
			.UntilClients(TEXT("Wait for 2 tag->float entries"), [](FDirectMapTestState& ClientState) -> bool
			{
				for (TActorIterator<AArcMassTestDirectMapActor> It(ClientState.World); It; ++It)
				{
					if (It->TagFloatMap.Entries.Num() == 2)
					{
						ClientState.Actor = *It;
						return true;
					}
				}
				return false;
			})
			.ThenClients(TEXT("Verify tag->float entries"), [this](FDirectMapTestState& ClientState)
			{
				ASSERT_THAT(IsNotNull(ClientState.Actor));
				ASSERT_THAT(AreEqual(ClientState.Actor->TagFloatMap.Entries.Num(), 2));

				const FGameplayTag TagA = ArcMassMapTestPrivate::MakeTestTag(FName(TEXT("Test.Map.A")));
				const FGameplayTag TagB = ArcMassMapTestPrivate::MakeTestTag(FName(TEXT("Test.Map.B")));

				const float* VA = ClientState.Actor->TagFloatMap.Entries.Find(TagA);
				ASSERT_THAT(IsNotNull(VA));
				ASSERT_THAT(IsNear(*VA, 1.5f, 0.01f));

				const float* VB = ClientState.Actor->TagFloatMap.Entries.Find(TagB);
				ASSERT_THAT(IsNotNull(VB));
				ASSERT_THAT(IsNear(*VB, 2.5f, 0.01f));
			});
	}

	// -----------------------------------------------------------------------
	// Test 6: FInstancedStruct value round-trips (dynamic state + CollectNetReferences).
	// Key: FGameplayTag (substituted for FArcItemId -- see file-header note).
	// This exercises inner Clone/Free + CollectNetReferences in the map serializer.
	// -----------------------------------------------------------------------
	TEST_METHOD(InstancedStructValue_RoundTrips)
	{
		Network
			.ThenServer(TEXT("Spawn actor, add tag->instanced-struct entry"), [this](FDirectMapTestState& ServerState)
			{
				ServerState.Actor = ServerState.World->SpawnActor<AArcMassTestDirectMapActor>();
				ASSERT_THAT(IsNotNull(ServerState.Actor));
				ArcMassMapTestPrivate::EnableDeltaCompression(ServerState.Actor);

				const FGameplayTag TagK = ArcMassMapTestPrivate::MakeTestTag(FName(TEXT("Test.Map.IS")));
				ASSERT_THAT(IsTrue(TagK.IsValid()));

				FArcMassTestNestedData Payload;
				Payload.Position = FVector(10.0, 20.0, 30.0);
				Payload.Health   = 75.f;
				Payload.Tag      = FName(TEXT("InstancedPayload"));

				ServerState.Actor->TagInstancedStructMap.Entries.Add(TagK, FInstancedStruct::Make(Payload));

				UE_LOGF(LogTemp, Log,"IrisMap InstancedStruct: server added tag->instanced-struct (Health=75)");
			})
			.UntilClients(TEXT("Wait for tag->instanced-struct entry"), [](FDirectMapTestState& ClientState) -> bool
			{
				for (TActorIterator<AArcMassTestDirectMapActor> It(ClientState.World); It; ++It)
				{
					if (It->TagInstancedStructMap.Entries.Num() == 1)
					{
						ClientState.Actor = *It;
						return true;
					}
				}
				return false;
			})
			.ThenClients(TEXT("Verify instanced-struct value"), [this](FDirectMapTestState& ClientState)
			{
				ASSERT_THAT(IsNotNull(ClientState.Actor));
				ASSERT_THAT(AreEqual(ClientState.Actor->TagInstancedStructMap.Entries.Num(), 1));

				const FGameplayTag TagK = ArcMassMapTestPrivate::MakeTestTag(FName(TEXT("Test.Map.IS")));
				const FInstancedStruct* Entry = ClientState.Actor->TagInstancedStructMap.Entries.Find(TagK);
				ASSERT_THAT(IsNotNull(Entry));
				ASSERT_THAT(IsTrue(Entry->IsValid()));

				const FArcMassTestNestedData* Data = Entry->GetPtr<FArcMassTestNestedData>();
				ASSERT_THAT(IsNotNull(Data));
				ASSERT_THAT(IsNear(Data->Position.X, 10.0, 0.1));
				ASSERT_THAT(IsNear(Data->Health, 75.f, 0.01f));
				ASSERT_THAT(AreEqual(Data->Tag, FName(TEXT("InstancedPayload"))));

				UE_LOGF(LogTemp, Log,"IrisMap InstancedStruct: client verified Health=%.1f Tag=%ls", Data->Health, *Data->Tag.ToString());
			});
	}

	// -----------------------------------------------------------------------
	// Test 7: Server populates map before client converges (exercises initial full-state send
	// via SerializeDelta with null Prev baseline -- equivalent to late-join).
	// Note: FNetworkComponentBuilder does not support adding clients after world start. True
	// late-join (client added post-population) is not supported by this test harness.
	// The initial-state code path (Args.Prev == nullptr -> all entries are "added") is the
	// same code path that a real late-joining client would hit.
	// -----------------------------------------------------------------------
	TEST_METHOD(LateJoin_ReceivesFullMap)
	{
		Network
			.ThenServer(TEXT("Populate map before client convergence"), [this](FDirectMapTestState& ServerState)
			{
				ServerState.Actor = ServerState.World->SpawnActor<AArcMassTestDirectMapActor>();
				ASSERT_THAT(IsNotNull(ServerState.Actor));
				ArcMassMapTestPrivate::EnableDeltaCompression(ServerState.Actor);

				// Add several entries so the initial send covers multiple entries.
				FArcMassTestReplicatedItem V;
				for (int32 i = 1; i <= 5; ++i)
				{
					V.ItemId = i;
					V.Value  = i * 11;
					ServerState.Actor->IntStructMap.Entries.Add(i, V);
				}
				UE_LOGF(LogTemp, Log,"IrisMap LateJoin: server populated 5 entries");
			})
			.UntilClients(TEXT("Client receives all 5 entries via initial state"), [](FDirectMapTestState& ClientState) -> bool
			{
				for (TActorIterator<AArcMassTestDirectMapActor> It(ClientState.World); It; ++It)
				{
					if (It->IntStructMap.Entries.Num() == 5)
					{
						ClientState.Actor = *It;
						return true;
					}
				}
				return false;
			})
			.ThenClients(TEXT("Verify all 5 entries correct"), [this](FDirectMapTestState& ClientState)
			{
				ASSERT_THAT(IsNotNull(ClientState.Actor));
				ASSERT_THAT(AreEqual(ClientState.Actor->IntStructMap.Entries.Num(), 5));

				for (int32 i = 1; i <= 5; ++i)
				{
					const FArcMassTestReplicatedItem* V = ClientState.Actor->IntStructMap.Entries.Find(i);
					ASSERT_THAT(IsNotNull(V));
					ASSERT_THAT(AreEqual(V->Value, i * 11));
				}
				UE_LOGF(LogTemp, Log,"IrisMap LateJoin: client verified all 5 entries");
			});
	}
};

// ---------------------------------------------------------------------------
// NETWORK_TEST_CLASS — ArcMassReplication.IrisMap.TwoClients
// Separate fixture so WithClients(2) can be set in BEFORE_EACH without conflicting
// with the single-client tests above (Build() cannot be called twice on a Network
// component within a single TEST_METHOD body).
// ---------------------------------------------------------------------------

NETWORK_TEST_CLASS(ArcIrisReplicatedMapTwoClientsTests, "ArcMassReplication.IrisMap")
{
	FPIENetworkComponent<FDirectMapTestState> Network{ TestRunner, TestCommandBuilder, bInitializing };

	inline static TSharedPtr<FScopedTestEnvironment> TestEnvironment{ nullptr };

	BEFORE_ALL()
	{
		TestEnvironment = FScopedTestEnvironment::Get();
		TestEnvironment->SetConsoleVariableValue(TEXT("net.Iris.UseIrisReplication"), TEXT("1"));
		TestEnvironment->SetConsoleVariableValue(TEXT("net.Iris.EnableDeltaCompression"), TEXT("1"));
	}

	BEFORE_EACH()
	{
		TestRunner->AddExpectedError(TEXT("FMassEntityQuery::CacheArchetypes"), EAutomationExpectedErrorFlags::Contains, -1);
		TestRunner->AddExpectedError(TEXT("SendRPC"), EAutomationExpectedErrorFlags::Contains, -1);

		FNetworkComponentBuilder<FDirectMapTestState>()
			.WithClients(2)
			.AsDedicatedServer()
			.WithGameInstanceClass(UGameInstance::StaticClass())
			.WithGameMode(AGameModeBase::StaticClass())
			.Build(Network);
	}

	AFTER_ALL()
	{
		TestEnvironment = nullptr;
	}

	// -----------------------------------------------------------------------
	// Test: Two clients both converge on the same map state.
	// -----------------------------------------------------------------------
	TEST_METHOD(TwoClients_BothConverge)
	{
		Network
			.ThenServer(TEXT("Spawn actor, add 2 entries"), [this](FDirectMapTestState& ServerState)
			{
				ServerState.Actor = ServerState.World->SpawnActor<AArcMassTestDirectMapActor>();
				ASSERT_THAT(IsNotNull(ServerState.Actor));
				ArcMassMapTestPrivate::EnableDeltaCompression(ServerState.Actor);

				FArcMassTestReplicatedItem A;
				A.ItemId = 7; A.Value = 70;
				ServerState.Actor->IntStructMap.Entries.Add(7, A);

				FArcMassTestReplicatedItem B;
				B.ItemId = 8; B.Value = 80;
				ServerState.Actor->IntStructMap.Entries.Add(8, B);
			})
			.UntilClients(TEXT("Both clients see 2 entries"), [](FDirectMapTestState& ClientState) -> bool
			{
				for (TActorIterator<AArcMassTestDirectMapActor> It(ClientState.World); It; ++It)
				{
					if (It->IntStructMap.Entries.Num() == 2)
					{
						ClientState.Actor = *It;
						return true;
					}
				}
				return false;
			})
			.ThenClients(TEXT("Verify both clients have correct entries"), [this](FDirectMapTestState& ClientState)
			{
				ASSERT_THAT(IsNotNull(ClientState.Actor));
				ASSERT_THAT(AreEqual(ClientState.Actor->IntStructMap.Entries.Num(), 2));

				const FArcMassTestReplicatedItem* A = ClientState.Actor->IntStructMap.Entries.Find(7);
				ASSERT_THAT(IsNotNull(A));
				ASSERT_THAT(AreEqual(A->Value, 70));

				const FArcMassTestReplicatedItem* B = ClientState.Actor->IntStructMap.Entries.Find(8);
				ASSERT_THAT(IsNotNull(B));
				ASSERT_THAT(AreEqual(B->Value, 80));
			});
	}
};

#endif // ENABLE_PIE_NETWORK_TEST && UE_WITH_IRIS
