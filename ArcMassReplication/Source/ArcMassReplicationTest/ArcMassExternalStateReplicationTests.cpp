// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "CQTestSettings.h"
#include "Components/PIENetworkComponent.h"
#include "Replication/ArcMassEntityReplicationProxy.h"
#include "Replication/ArcMassReplicationDescriptorSet.h"
#include "Subsystem/ArcMassEntityReplicationSubsystem.h"
#include "Fragments/ArcMassNetId.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcMassTestPayloadFragment.h"
#include "ArcMassTestComplexFragment.h"
#include "ArcMassTestStatsFragment.h"
#include "ArcMassTestItemFragment.h"
#include "ArcMassTestEntityHandleActor.h"
#include "ArcMassTestSparseFragment.h"
#include "ArcMassTestIrisSerializedFragment.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Tests/AutomationCommon.h"
#include "MassEntityManager.h"
#include "MassEntityUtils.h"
#include "MassEntityView.h"
#include "MassEntityConfigAsset.h"
#include "ArcMassTestReplicationTrait.h"
#include "Traits/ArcMassEntityReplicationTrait.h"
#include "Fragments/ArcMassReplicationFilter.h"

#if ENABLE_PIE_NETWORK_TEST && UE_WITH_IRIS

// ---------------------------------------------------------------------------
// FMassEntityHandle NetSerializer — round-trip replication test
// ---------------------------------------------------------------------------

NETWORK_TEST_CLASS(ArcMassEntityHandleReplication, "ArcMassReplication.EntityHandle")
{
	struct FHandleTestState : public FBasePIENetworkComponentState
	{
		AArcMassTestEntityHandleActor* HandleActor = nullptr;
		AArcMassEntityReplicationProxy* Proxy = nullptr;
		FMassEntityHandle ServerEntity;
		FArcMassNetId AllocatedNetId;
	};

	FPIENetworkComponent<FHandleTestState> Network{ TestRunner, TestCommandBuilder, bInitializing };

	inline static TSharedPtr<FScopedTestEnvironment> TestEnvironment{ nullptr };
	BEFORE_ALL()
	{
		TestEnvironment = FScopedTestEnvironment::Get();
		TestEnvironment->SetConsoleVariableValue(TEXT("net.Iris.UseIrisReplication"), TEXT("1"));
	}

	BEFORE_EACH()
	{
		TestRunner->AddExpectedError(TEXT("FMassEntityQuery::CacheArchetypes"), EAutomationExpectedErrorFlags::Contains, -1);
		TestRunner->AddExpectedError(TEXT("SendRPC"), EAutomationExpectedErrorFlags::Contains, -1);

		FNetworkComponentBuilder<FHandleTestState>()
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

	TEST_METHOD(EntityHandle_ReplicatesToClient_ResolvesCorrectly)
	{
		static constexpr uint32 TestPayload = 99;

		Network
			.ThenServer(TEXT("Create entity, replicate via proxy, spawn handle actor"), [this](FHandleTestState& ServerState)
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ServerState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				ASSERT_THAT(IsNotNull(Subsystem));

				FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*ServerState.World);

				FMassFragmentBitSet FragmentBitSet;
				FragmentBitSet.Add(*FArcMassTestPayloadFragment::StaticStruct());
				FMassArchetypeHandle Archetype = EntityManager.CreateArchetype(FragmentBitSet, {});
				FMassEntityHandle Entity = EntityManager.CreateEntity(Archetype);
				ServerState.ServerEntity = Entity;

				FArcMassTestPayloadFragment& PayloadFrag = EntityManager.GetFragmentDataChecked<FArcMassTestPayloadFragment>(Entity);
				PayloadFrag.Payload = TestPayload;

				FArcMassNetId NetId = Subsystem->AllocateNetId();
				ServerState.AllocatedNetId = NetId;
				Subsystem->RegisterEntityNetId(NetId, Entity);

				ServerState.Proxy = Subsystem->SpawnProxyOfClass<AArcMassTestPayloadProxy>();
				ASSERT_THAT(IsNotNull(ServerState.Proxy));

				TArray<FInstancedStruct> FragmentSlots;
				FragmentSlots.Add(FInstancedStruct::Make(PayloadFrag));
				ServerState.Proxy->SetEntityFragments(NetId, FragmentSlots);

				ServerState.HandleActor = ServerState.World->SpawnActor<AArcMassTestEntityHandleActor>();
				ASSERT_THAT(IsNotNull(ServerState.HandleActor));

				ServerState.HandleActor->ReplicatedHandle = Entity;
			})
			.UntilClients(TEXT("Wait for entity to replicate to client"), [](FHandleTestState& ClientState) -> bool
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				if (!Subsystem)
				{
					return false;
				}

				FMassEntityHandle Entity = Subsystem->FindEntityByNetId(FArcMassNetId(1));
				return Entity.IsSet();
			})
			.UntilClients(TEXT("Wait for handle to resolve on client"), [](FHandleTestState& ClientState) -> bool
			{
				for (TActorIterator<AArcMassTestEntityHandleActor> It(ClientState.World); It; ++It)
				{
					FMassEntityHandle ClientHandle = It->ReplicatedHandle;
					if (ClientHandle.IsSet())
					{
						ClientState.HandleActor = *It;
						return true;
					}
				}
				return false;
			})
			.ThenClients(TEXT("Verify handle resolves to valid entity with correct payload"), [this](FHandleTestState& ClientState)
			{
				ASSERT_THAT(IsNotNull(ClientState.HandleActor));

				FMassEntityHandle ClientHandle = ClientState.HandleActor->ReplicatedHandle;
				ASSERT_THAT(IsTrue(ClientHandle.IsSet()));

				FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*ClientState.World);
				ASSERT_THAT(IsTrue(EntityManager.IsEntityValid(ClientHandle)));

				const FArcMassTestPayloadFragment* Frag =
					EntityManager.GetFragmentDataPtr<FArcMassTestPayloadFragment>(ClientHandle);
				ASSERT_THAT(IsNotNull(Frag));
				ASSERT_THAT(AreEqual(Frag->Payload, TestPayload));
			});
	}
};

// ---------------------------------------------------------------------------
// Sparse fragment replication test
// ---------------------------------------------------------------------------

NETWORK_TEST_CLASS(ArcMassSparseFragmentReplication, "ArcMassReplication.SparseFragment")
{
	struct FSparseTestState : public FBasePIENetworkComponentState
	{
		AArcMassTestSparseProxy* Proxy = nullptr;
		FMassEntityHandle ServerEntity;
		FArcMassNetId AllocatedNetId;
	};

	FPIENetworkComponent<FSparseTestState> Network{ TestRunner, TestCommandBuilder, bInitializing };

	inline static TSharedPtr<FScopedTestEnvironment> TestEnvironment{ nullptr };
	BEFORE_ALL()
	{
		TestEnvironment = FScopedTestEnvironment::Get();
		TestEnvironment->SetConsoleVariableValue(TEXT("net.Iris.UseIrisReplication"), TEXT("1"));
	}

	BEFORE_EACH()
	{
		TestRunner->AddExpectedError(TEXT("FMassEntityQuery::CacheArchetypes"), EAutomationExpectedErrorFlags::Contains, -1);
		TestRunner->AddExpectedError(TEXT("SendRPC"), EAutomationExpectedErrorFlags::Contains, -1);

		FNetworkComponentBuilder<FSparseTestState>()
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

	TEST_METHOD(SparseFragment_ReplicatesToClient)
	{
		static constexpr int32 TestHealth = 77;
		static constexpr float TestSpeed = 3.5f;
		static constexpr uint32 TestPayload = 123;

		Network
			.ThenServer(TEXT("Create entity with sparse fragment, replicate via proxy"), [this](FSparseTestState& ServerState)
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ServerState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				ASSERT_THAT(IsNotNull(Subsystem));

				FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*ServerState.World);

				FMassFragmentBitSet FragmentBitSet;
				FragmentBitSet.Add(*FArcMassTestPayloadFragment::StaticStruct());
				FMassArchetypeHandle Archetype = EntityManager.CreateArchetype(FragmentBitSet, {});
				FMassEntityHandle Entity = EntityManager.CreateEntity(Archetype);
				ServerState.ServerEntity = Entity;

				FArcMassTestPayloadFragment& PayloadFrag = EntityManager.GetFragmentDataChecked<FArcMassTestPayloadFragment>(Entity);
				PayloadFrag.Payload = TestPayload;

				FArcMassTestSparseFragment& SparseFrag = EntityManager.AddSparseElementToEntity<FArcMassTestSparseFragment>(Entity);
				SparseFrag.Health = TestHealth;
				SparseFrag.Speed = TestSpeed;

				FArcMassNetId NetId = Subsystem->AllocateNetId();
				ServerState.AllocatedNetId = NetId;
				Subsystem->RegisterEntityNetId(NetId, Entity);

				ServerState.Proxy = Subsystem->SpawnProxyOfClass<AArcMassTestSparseProxy>();
				ASSERT_THAT(IsNotNull(ServerState.Proxy));

				TArray<FInstancedStruct> FragmentSlots;
				FragmentSlots.Add(FInstancedStruct::Make(PayloadFrag));
				FragmentSlots.Add(FInstancedStruct::Make(SparseFrag));
				ServerState.Proxy->SetEntityFragments(NetId, FragmentSlots);
			})
			.UntilClients(TEXT("Wait for entity on client"), [](FSparseTestState& ClientState) -> bool
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				if (!Subsystem)
				{
					return false;
				}

				FMassEntityHandle Entity = Subsystem->FindEntityByNetId(FArcMassNetId(1));
				return Entity.IsSet();
			})
			.ThenClients(TEXT("Verify sparse fragment data on client"), [this](FSparseTestState& ClientState)
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				ASSERT_THAT(IsNotNull(Subsystem));

				FMassEntityHandle Entity = Subsystem->FindEntityByNetId(FArcMassNetId(1));
				ASSERT_THAT(IsTrue(Entity.IsSet()));

				FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*ClientState.World);
				ASSERT_THAT(IsTrue(EntityManager.IsEntityValid(Entity)));

				FMassEntityView EntityView(EntityManager, Entity);

				const FArcMassTestPayloadFragment* PayloadFrag =
					EntityView.GetFragmentDataPtr<FArcMassTestPayloadFragment>();
				ASSERT_THAT(IsNotNull(PayloadFrag));
				ASSERT_THAT(AreEqual(PayloadFrag->Payload, TestPayload));

				const FArcMassTestSparseFragment* SparseFrag =
					EntityView.GetSparseFragmentDataPtr<FArcMassTestSparseFragment>();
				ASSERT_THAT(IsNotNull(SparseFrag));
				ASSERT_THAT(AreEqual(SparseFrag->Health, TestHealth));
				ASSERT_THAT(IsNear(SparseFrag->Speed, TestSpeed, 0.01f));
			});
	}
};

// ---------------------------------------------------------------------------
// Config asset-based entity spawning test
// ---------------------------------------------------------------------------
// Tests OnClientEntityAdded directly rather than full replication round-trip
// because Iris cannot replicate TObjectPtr to transient-package objects.

NETWORK_TEST_CLASS(ArcMassConfigBasedReplication, "ArcMassReplication.ConfigAsset")
{
	struct FConfigTestState : public FBasePIENetworkComponentState {};

	FPIENetworkComponent<FConfigTestState> Network{ TestRunner, TestCommandBuilder, bInitializing };

	UMassEntityConfigAsset* TestConfigAsset = nullptr;

	inline static TSharedPtr<FScopedTestEnvironment> TestEnvironment{ nullptr };
	BEFORE_ALL()
	{
		TestEnvironment = FScopedTestEnvironment::Get();
		TestEnvironment->SetConsoleVariableValue(TEXT("net.Iris.UseIrisReplication"), TEXT("1"));
	}

	BEFORE_EACH()
	{
		TestRunner->AddExpectedError(TEXT("FMassEntityQuery::CacheArchetypes"), EAutomationExpectedErrorFlags::Contains, -1);
		TestRunner->AddExpectedError(TEXT("SendRPC"), EAutomationExpectedErrorFlags::Contains, -1);

		TestConfigAsset = NewObject<UMassEntityConfigAsset>();
		TestConfigAsset->AddToRoot();

		UArcMassTestReplicationTrait* TestTrait = NewObject<UArcMassTestReplicationTrait>(TestConfigAsset);
		TestConfigAsset->GetMutableConfig().AddTrait(*TestTrait);

		FNetworkComponentBuilder<FConfigTestState>()
			.WithClients(1)
			.AsDedicatedServer()
			.WithGameInstanceClass(UGameInstance::StaticClass())
			.WithGameMode(AGameModeBase::StaticClass())
			.Build(Network);
	}

	AFTER_EACH()
	{
		if (TestConfigAsset)
		{
			TestConfigAsset->RemoveFromRoot();
			TestConfigAsset = nullptr;
		}
	}

	AFTER_ALL()
	{
		TestEnvironment = nullptr;
	}

	TEST_METHOD(ConfigAsset_ClientEntity_HasFullTemplate)
	{
		static constexpr uint32 TestPayload = 55;

		Network
			.ThenClients(TEXT("Call OnClientEntityAdded with config asset, verify full template"), [this](FConfigTestState& ClientState)
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				ASSERT_THAT(IsNotNull(Subsystem));

				TArray<const UScriptStruct*> FragmentTypes;
				FragmentTypes.Add(FArcMassTestPayloadFragment::StaticStruct());

				ArcMassReplication::FArcMassReplicationDescriptorSet DescSet =
					ArcMassReplication::FArcMassReplicationDescriptorSet::Build(FragmentTypes);

				FArcMassTestPayloadFragment PayloadFrag;
				PayloadFrag.Payload = TestPayload;

				TArray<FInstancedStruct> FragmentSlots;
				FragmentSlots.Add(FInstancedStruct::Make(PayloadFrag));

				FArcMassNetId NetId(1);
				Subsystem->OnClientEntityAdded(NetId, FragmentSlots, &DescSet, TestConfigAsset);

				FMassEntityHandle Entity = Subsystem->FindEntityByNetId(NetId);
				ASSERT_THAT(IsTrue(Entity.IsSet()));

				FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*ClientState.World);
				ASSERT_THAT(IsTrue(EntityManager.IsEntityValid(Entity)));

				FMassEntityView EntityView(EntityManager, Entity);

				const FArcMassTestPayloadFragment* Frag =
					EntityView.GetFragmentDataPtr<FArcMassTestPayloadFragment>();
				ASSERT_THAT(IsNotNull(Frag));
				ASSERT_THAT(AreEqual(Frag->Payload, TestPayload));

				const bool bHasTemplateTag = EntityView.HasTag<FArcMassTestTemplateTag>();
				ASSERT_THAT(IsTrue(bHasTemplateTag));
			});
	}
};

// ---------------------------------------------------------------------------
// Iris entity array end-to-end tests
// ---------------------------------------------------------------------------

NETWORK_TEST_CLASS(ArcIrisEntityArrayReplication, "ArcMassReplication.IrisEntityArray")
{
	struct FIrisTestState : public FBasePIENetworkComponentState
	{
		AArcMassEntityReplicationProxy* Proxy = nullptr;
		FArcMassNetId AllocatedNetId;
	};

	FPIENetworkComponent<FIrisTestState> Network{ TestRunner, TestCommandBuilder, bInitializing };

	inline static TSharedPtr<FScopedTestEnvironment> TestEnvironment{ nullptr };
	BEFORE_ALL()
	{
		TestEnvironment = FScopedTestEnvironment::Get();
		TestEnvironment->SetConsoleVariableValue(TEXT("net.Iris.UseIrisReplication"), TEXT("1"));
	}

	BEFORE_EACH()
	{
		TestRunner->AddExpectedError(TEXT("FMassEntityQuery::CacheArchetypes"), EAutomationExpectedErrorFlags::Contains, -1);
		TestRunner->AddExpectedError(TEXT("SendRPC"), EAutomationExpectedErrorFlags::Contains, -1);

		FNetworkComponentBuilder<FIrisTestState>()
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

	TEST_METHOD(SingleProperty_ReplicatesToClient)
	{
		static constexpr uint32 TestPayload = 42;

		Network
			.ThenServer(TEXT("Spawn proxy, set single-property fragment"), [this](FIrisTestState& ServerState)
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ServerState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				ASSERT_THAT(IsNotNull(Subsystem));

				ServerState.Proxy = Subsystem->SpawnProxyOfClass<AArcMassTestPayloadProxy>();
				ASSERT_THAT(IsNotNull(ServerState.Proxy));

				ServerState.AllocatedNetId = Subsystem->AllocateNetId();

				FArcMassTestPayloadFragment Frag;
				Frag.Payload = TestPayload;

				TArray<FInstancedStruct> Slots;
				Slots.Add(FInstancedStruct::Make(Frag));
				ServerState.Proxy->SetEntityFragments(ServerState.AllocatedNetId, Slots);
			})
			.UntilClients(TEXT("Wait for entity on client"), [](FIrisTestState& ClientState) -> bool
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				if (!Subsystem)
				{
					return false;
				}

				FMassEntityHandle Entity = Subsystem->FindEntityByNetId(FArcMassNetId(1));
				return Entity.IsSet();
			})
			.ThenClients(TEXT("Verify fragment value"), [this](FIrisTestState& ClientState)
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				ASSERT_THAT(IsNotNull(Subsystem));

				FMassEntityHandle Entity = Subsystem->FindEntityByNetId(FArcMassNetId(1));
				ASSERT_THAT(IsTrue(Entity.IsSet()));

				FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*ClientState.World);
				FMassEntityView EntityView(EntityManager, Entity);

				const FArcMassTestPayloadFragment* Frag =
					EntityView.GetFragmentDataPtr<FArcMassTestPayloadFragment>();
				ASSERT_THAT(IsNotNull(Frag));
				ASSERT_THAT(AreEqual(Frag->Payload, TestPayload));
			});
	}

	TEST_METHOD(MultiProperty_ReplicatesToClient)
	{
		Network
			.ThenServer(TEXT("Spawn proxy, set multi-property fragment"), [this](FIrisTestState& ServerState)
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ServerState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				ASSERT_THAT(IsNotNull(Subsystem));

				ServerState.Proxy = Subsystem->SpawnProxyOfClass<AArcMassTestStatsProxy>();
				ASSERT_THAT(IsNotNull(ServerState.Proxy));

				ServerState.AllocatedNetId = Subsystem->AllocateNetId();

				FArcMassTestStatsFragment Frag;
				Frag.Health = 100;
				Frag.Speed = 5.5f;
				Frag.Armor = 25;

				TArray<FInstancedStruct> Slots;
				Slots.Add(FInstancedStruct::Make(Frag));
				ServerState.Proxy->SetEntityFragments(ServerState.AllocatedNetId, Slots);
			})
			.UntilClients(TEXT("Wait for entity on client"), [](FIrisTestState& ClientState) -> bool
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				if (!Subsystem)
				{
					return false;
				}
				FMassEntityHandle Entity = Subsystem->FindEntityByNetId(FArcMassNetId(1));
				if (!Entity.IsSet())
				{
					return false;
				}
				FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*ClientState.World);
				FMassEntityView EntityView(EntityManager, Entity);
				const FArcMassTestStatsFragment* Frag = EntityView.GetFragmentDataPtr<FArcMassTestStatsFragment>();
				return Frag && Frag->Health == 100;
			})
			.ThenClients(TEXT("Verify all properties"), [this](FIrisTestState& ClientState)
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				FMassEntityHandle Entity = Subsystem->FindEntityByNetId(FArcMassNetId(1));
				FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*ClientState.World);
				FMassEntityView EntityView(EntityManager, Entity);

				const FArcMassTestStatsFragment* Frag = EntityView.GetFragmentDataPtr<FArcMassTestStatsFragment>();
				ASSERT_THAT(IsNotNull(Frag));
				ASSERT_THAT(AreEqual(Frag->Health, 100));
				ASSERT_THAT(IsNear(Frag->Speed, 5.5f, 0.01f));
				ASSERT_THAT(AreEqual(Frag->Armor, 25));
			});
	}

	TEST_METHOD(PropertyUpdate_ReplicatesToClient)
	{
		Network
			.ThenServer(TEXT("Spawn proxy, set initial values"), [this](FIrisTestState& ServerState)
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ServerState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				ASSERT_THAT(IsNotNull(Subsystem));

				ServerState.Proxy = Subsystem->SpawnProxyOfClass<AArcMassTestStatsProxy>();
				ASSERT_THAT(IsNotNull(ServerState.Proxy));

				ServerState.AllocatedNetId = Subsystem->AllocateNetId();

				FArcMassTestStatsFragment Frag;
				Frag.Health = 100;
				Frag.Speed = 5.0f;
				Frag.Armor = 10;

				TArray<FInstancedStruct> Slots;
				Slots.Add(FInstancedStruct::Make(Frag));
				ServerState.Proxy->SetEntityFragments(ServerState.AllocatedNetId, Slots);
			})
			.UntilClients(TEXT("Wait for initial data"), [](FIrisTestState& ClientState) -> bool
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				if (!Subsystem)
				{
					return false;
				}
				FMassEntityHandle Entity = Subsystem->FindEntityByNetId(FArcMassNetId(1));
				if (!Entity.IsSet())
				{
					return false;
				}
				FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*ClientState.World);
				FMassEntityView EntityView(EntityManager, Entity);
				const FArcMassTestStatsFragment* Frag = EntityView.GetFragmentDataPtr<FArcMassTestStatsFragment>();
				return Frag && Frag->Health == 100;
			})
			.ThenServer(TEXT("Update values"), [this](FIrisTestState& ServerState)
			{
				FArcMassTestStatsFragment Updated;
				Updated.Health = 75;
				Updated.Speed = 8.0f;
				Updated.Armor = 10;

				TArray<FInstancedStruct> Slots;
				Slots.Add(FInstancedStruct::Make(Updated));
				ServerState.Proxy->SetEntityFragments(ServerState.AllocatedNetId, Slots);
			})
			.UntilClients(TEXT("Wait for updated data"), [](FIrisTestState& ClientState) -> bool
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				if (!Subsystem)
				{
					return false;
				}
				FMassEntityHandle Entity = Subsystem->FindEntityByNetId(FArcMassNetId(1));
				if (!Entity.IsSet())
				{
					return false;
				}
				FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*ClientState.World);
				FMassEntityView EntityView(EntityManager, Entity);
				const FArcMassTestStatsFragment* Frag = EntityView.GetFragmentDataPtr<FArcMassTestStatsFragment>();
				return Frag && Frag->Health == 75;
			})
			.ThenClients(TEXT("Verify updated values"), [this](FIrisTestState& ClientState)
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				FMassEntityHandle Entity = Subsystem->FindEntityByNetId(FArcMassNetId(1));
				FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*ClientState.World);
				FMassEntityView EntityView(EntityManager, Entity);

				const FArcMassTestStatsFragment* Frag = EntityView.GetFragmentDataPtr<FArcMassTestStatsFragment>();
				ASSERT_THAT(IsNotNull(Frag));
				ASSERT_THAT(AreEqual(Frag->Health, 75));
				ASSERT_THAT(IsNear(Frag->Speed, 8.0f, 0.01f));
				ASSERT_THAT(AreEqual(Frag->Armor, 10));
			});
	}

	TEST_METHOD(MultiFragment_ReplicatesToClient)
	{
		static constexpr uint32 TestPayload = 55;

		Network
			.ThenServer(TEXT("Spawn proxy with two fragments"), [this](FIrisTestState& ServerState)
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ServerState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				ASSERT_THAT(IsNotNull(Subsystem));

				ServerState.Proxy = Subsystem->SpawnProxyOfClass<AArcMassTestMultiFragmentProxy>();
				ASSERT_THAT(IsNotNull(ServerState.Proxy));

				ServerState.AllocatedNetId = Subsystem->AllocateNetId();

				FArcMassTestPayloadFragment PayloadFrag;
				PayloadFrag.Payload = TestPayload;

				FArcMassTestStatsFragment StatsFrag;
				StatsFrag.Health = 80;
				StatsFrag.Speed = 3.0f;
				StatsFrag.Armor = 15;

				TArray<FInstancedStruct> Slots;
				Slots.Add(FInstancedStruct::Make(PayloadFrag));
				Slots.Add(FInstancedStruct::Make(StatsFrag));
				ServerState.Proxy->SetEntityFragments(ServerState.AllocatedNetId, Slots);
			})
			.UntilClients(TEXT("Wait for entity"), [](FIrisTestState& ClientState) -> bool
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				if (!Subsystem) return false;
				FMassEntityHandle Entity = Subsystem->FindEntityByNetId(FArcMassNetId(1));
				return Entity.IsSet();
			})
			.ThenClients(TEXT("Verify both fragments"), [this](FIrisTestState& ClientState)
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				FMassEntityHandle Entity = Subsystem->FindEntityByNetId(FArcMassNetId(1));
				FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*ClientState.World);
				FMassEntityView EntityView(EntityManager, Entity);

				const FArcMassTestPayloadFragment* PayloadFrag =
					EntityView.GetFragmentDataPtr<FArcMassTestPayloadFragment>();
				ASSERT_THAT(IsNotNull(PayloadFrag));
				ASSERT_THAT(AreEqual(PayloadFrag->Payload, TestPayload));

				const FArcMassTestStatsFragment* StatsFrag =
					EntityView.GetFragmentDataPtr<FArcMassTestStatsFragment>();
				ASSERT_THAT(IsNotNull(StatsFrag));
				ASSERT_THAT(AreEqual(StatsFrag->Health, 80));
				ASSERT_THAT(IsNear(StatsFrag->Speed, 3.0f, 0.01f));
				ASSERT_THAT(AreEqual(StatsFrag->Armor, 15));
			});
	}

	TEST_METHOD(SelectiveDirty_OnlyChangedEntityReplicates)
	{
		Network
			.ThenServer(TEXT("Spawn proxy, add 3 entities"), [this](FIrisTestState& ServerState)
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ServerState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				ASSERT_THAT(IsNotNull(Subsystem));

				ServerState.Proxy = Subsystem->SpawnProxyOfClass<AArcMassTestPayloadProxy>();
				ASSERT_THAT(IsNotNull(ServerState.Proxy));

				for (uint32 Idx = 0; Idx < 3; ++Idx)
				{
					FArcMassNetId NetId = Subsystem->AllocateNetId();

					FArcMassTestPayloadFragment Frag;
					Frag.Payload = (Idx + 1) * 100;

					TArray<FInstancedStruct> Slots;
					Slots.Add(FInstancedStruct::Make(Frag));
					ServerState.Proxy->SetEntityFragments(NetId, Slots);
				}
			})
			.UntilClients(TEXT("Wait for all 3 entities"), [](FIrisTestState& ClientState) -> bool
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				if (!Subsystem)
				{
					return false;
				}
				return Subsystem->FindEntityByNetId(FArcMassNetId(1)).IsSet()
					&& Subsystem->FindEntityByNetId(FArcMassNetId(2)).IsSet()
					&& Subsystem->FindEntityByNetId(FArcMassNetId(3)).IsSet();
			})
			.ThenServer(TEXT("Update only entity 2"), [this](FIrisTestState& ServerState)
			{
				FArcMassTestPayloadFragment Updated;
				Updated.Payload = 999;

				TArray<FInstancedStruct> Slots;
				Slots.Add(FInstancedStruct::Make(Updated));
				ServerState.Proxy->SetEntityFragments(FArcMassNetId(2), Slots);
			})
			.UntilClients(TEXT("Wait for entity 2 change"), [](FIrisTestState& ClientState) -> bool
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				if (!Subsystem)
				{
					return false;
				}
				FMassEntityHandle Entity = Subsystem->FindEntityByNetId(FArcMassNetId(2));
				if (!Entity.IsSet())
				{
					return false;
				}
				FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*ClientState.World);
				FMassEntityView EntityView(EntityManager, Entity);
				const FArcMassTestPayloadFragment* Frag = EntityView.GetFragmentDataPtr<FArcMassTestPayloadFragment>();
				return Frag && Frag->Payload == 999;
			})
			.ThenClients(TEXT("Verify entity 2 changed, others unchanged"), [this](FIrisTestState& ClientState)
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*ClientState.World);

				FMassEntityHandle Entity1 = Subsystem->FindEntityByNetId(FArcMassNetId(1));
				FMassEntityView View1(EntityManager, Entity1);
				const FArcMassTestPayloadFragment* Frag1 = View1.GetFragmentDataPtr<FArcMassTestPayloadFragment>();
				ASSERT_THAT(IsNotNull(Frag1));
				ASSERT_THAT(AreEqual(Frag1->Payload, static_cast<uint32>(100)));

				FMassEntityHandle Entity2 = Subsystem->FindEntityByNetId(FArcMassNetId(2));
				FMassEntityView View2(EntityManager, Entity2);
				const FArcMassTestPayloadFragment* Frag2 = View2.GetFragmentDataPtr<FArcMassTestPayloadFragment>();
				ASSERT_THAT(IsNotNull(Frag2));
				ASSERT_THAT(AreEqual(Frag2->Payload, static_cast<uint32>(999)));

				FMassEntityHandle Entity3 = Subsystem->FindEntityByNetId(FArcMassNetId(3));
				FMassEntityView View3(EntityManager, Entity3);
				const FArcMassTestPayloadFragment* Frag3 = View3.GetFragmentDataPtr<FArcMassTestPayloadFragment>();
				ASSERT_THAT(IsNotNull(Frag3));
				ASSERT_THAT(AreEqual(Frag3->Payload, static_cast<uint32>(300)));
			});
	}

	TEST_METHOD(EntityRemoval_ReplicatesToClient)
	{
		Network
			.ThenServer(TEXT("Spawn proxy and add entity"), [this](FIrisTestState& ServerState)
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ServerState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				ASSERT_THAT(IsNotNull(Subsystem));

				ServerState.Proxy = Subsystem->SpawnProxyOfClass<AArcMassTestPayloadProxy>();
				ServerState.AllocatedNetId = Subsystem->AllocateNetId();

				FArcMassTestPayloadFragment Frag;
				Frag.Payload = 99;

				TArray<FInstancedStruct> Slots;
				Slots.Add(FInstancedStruct::Make(Frag));
				ServerState.Proxy->SetEntityFragments(ServerState.AllocatedNetId, Slots);
			})
			.UntilClients(TEXT("Wait for entity to arrive"), [](FIrisTestState& ClientState) -> bool
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				if (!Subsystem) return false;
				return Subsystem->FindEntityByNetId(FArcMassNetId(1)).IsSet();
			})
			.ThenServer(TEXT("Remove entity"), [this](FIrisTestState& ServerState)
			{
				ServerState.Proxy->RemoveEntity(ServerState.AllocatedNetId);
			})
			.UntilClients(TEXT("Wait for entity removal"), [](FIrisTestState& ClientState) -> bool
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				if (!Subsystem) return false;
				return !Subsystem->FindEntityByNetId(FArcMassNetId(1)).IsSet();
			})
			.ThenClients(TEXT("Verify entity is gone"), [this](FIrisTestState& ClientState)
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				FMassEntityHandle Entity = Subsystem->FindEntityByNetId(FArcMassNetId(1));
				ASSERT_THAT(IsFalse(Entity.IsSet()));
			});
	}
};

// ---------------------------------------------------------------------------
// Non-flat fragment (TArray field) replication via Iris descriptors
// ---------------------------------------------------------------------------

NETWORK_TEST_CLASS(ArcIrisItemArrayReplication, "ArcMassReplication.ItemArray")
{
	struct FItemTestState : public FBasePIENetworkComponentState
	{
		AArcMassTestItemProxy* Proxy = nullptr;
		FArcMassNetId AllocatedNetId;
	};

	FPIENetworkComponent<FItemTestState> Network{ TestRunner, TestCommandBuilder, bInitializing };

	inline static TSharedPtr<FScopedTestEnvironment> TestEnvironment{ nullptr };
	BEFORE_ALL()
	{
		TestEnvironment = FScopedTestEnvironment::Get();
		TestEnvironment->SetConsoleVariableValue(TEXT("net.Iris.UseIrisReplication"), TEXT("1"));
	}

	BEFORE_EACH()
	{
		TestRunner->AddExpectedError(TEXT("FMassEntityQuery::CacheArchetypes"), EAutomationExpectedErrorFlags::Contains, -1);
		TestRunner->AddExpectedError(TEXT("SendRPC"), EAutomationExpectedErrorFlags::Contains, -1);

		FNetworkComponentBuilder<FItemTestState>()
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

	TEST_METHOD(NonFlatFragment_WithTArray_ReplicatesToClient)
	{
		Network
			.ThenServer(TEXT("Spawn proxy, set fragment with TArray items"), [this](FItemTestState& ServerState)
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ServerState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				ASSERT_THAT(IsNotNull(Subsystem));

				ServerState.Proxy = Subsystem->SpawnProxyOfClass<AArcMassTestItemProxy>();
				ASSERT_THAT(IsNotNull(ServerState.Proxy));

				ServerState.AllocatedNetId = Subsystem->AllocateNetId();

				FArcMassTestItemFragment Frag;
				FArcMassTestReplicatedItem Item1;
				Item1.ItemId = 10;
				Item1.Value = 100;
				FArcMassTestReplicatedItem Item2;
				Item2.ItemId = 20;
				Item2.Value = 200;
				Frag.ReplicatedItems.AddItem(MoveTemp(Item1));
				Frag.ReplicatedItems.AddItem(MoveTemp(Item2));

				TArray<FInstancedStruct> Slots;
				Slots.Add(FInstancedStruct::Make(Frag));
				ServerState.Proxy->SetEntityFragments(ServerState.AllocatedNetId, Slots);
			})
			.UntilClients(TEXT("Wait for entity on client"), [](FItemTestState& ClientState) -> bool
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				if (!Subsystem)
				{
					return false;
				}
				FMassEntityHandle Entity = Subsystem->FindEntityByNetId(FArcMassNetId(1));
				return Entity.IsSet();
			})
			.ThenClients(TEXT("Verify TArray items replicated correctly"), [this](FItemTestState& ClientState)
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				FMassEntityHandle Entity = Subsystem->FindEntityByNetId(FArcMassNetId(1));
				FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*ClientState.World);
				FMassEntityView EntityView(EntityManager, Entity);

				const FArcMassTestItemFragment* Frag =
					EntityView.GetFragmentDataPtr<FArcMassTestItemFragment>();
				ASSERT_THAT(IsNotNull(Frag));
				ASSERT_THAT(AreEqual(Frag->ReplicatedItems.Items.Num(), 2));
				ASSERT_THAT(AreEqual(Frag->ReplicatedItems.Items[0].ItemId, 10));
				ASSERT_THAT(AreEqual(Frag->ReplicatedItems.Items[0].Value, 100));
				ASSERT_THAT(AreEqual(Frag->ReplicatedItems.Items[1].ItemId, 20));
				ASSERT_THAT(AreEqual(Frag->ReplicatedItems.Items[1].Value, 200));
			});
	}

	TEST_METHOD(NonFlatFragment_Update_ReplicatesToClient)
	{
		Network
			.ThenServer(TEXT("Spawn proxy with initial items"), [this](FItemTestState& ServerState)
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ServerState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				ASSERT_THAT(IsNotNull(Subsystem));

				ServerState.Proxy = Subsystem->SpawnProxyOfClass<AArcMassTestItemProxy>();
				ASSERT_THAT(IsNotNull(ServerState.Proxy));

				ServerState.AllocatedNetId = Subsystem->AllocateNetId();

				FArcMassTestItemFragment Frag;
				FArcMassTestReplicatedItem Item1;
				Item1.ItemId = 1;
				Item1.Value = 50;
				Frag.ReplicatedItems.AddItem(MoveTemp(Item1));

				TArray<FInstancedStruct> Slots;
				Slots.Add(FInstancedStruct::Make(Frag));
				ServerState.Proxy->SetEntityFragments(ServerState.AllocatedNetId, Slots);
			})
			.UntilClients(TEXT("Wait for initial data"), [](FItemTestState& ClientState) -> bool
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				if (!Subsystem) return false;
				FMassEntityHandle Entity = Subsystem->FindEntityByNetId(FArcMassNetId(1));
				if (!Entity.IsSet()) return false;
				FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*ClientState.World);
				FMassEntityView EntityView(EntityManager, Entity);
				const FArcMassTestItemFragment* Frag = EntityView.GetFragmentDataPtr<FArcMassTestItemFragment>();
				return Frag && Frag->ReplicatedItems.Items.Num() == 1;
			})
			.ThenServer(TEXT("Update: add second item, change first"), [this](FItemTestState& ServerState)
			{
				FArcMassTestItemFragment Updated;
				FArcMassTestReplicatedItem Item1;
				Item1.ItemId = 1;
				Item1.Value = 999;
				FArcMassTestReplicatedItem Item2;
				Item2.ItemId = 2;
				Item2.Value = 777;
				Updated.ReplicatedItems.AddItem(MoveTemp(Item1));
				Updated.ReplicatedItems.AddItem(MoveTemp(Item2));

				TArray<FInstancedStruct> Slots;
				Slots.Add(FInstancedStruct::Make(Updated));
				ServerState.Proxy->SetEntityFragments(ServerState.AllocatedNetId, Slots);
			})
			.UntilClients(TEXT("Wait for updated data"), [](FItemTestState& ClientState) -> bool
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				if (!Subsystem) return false;
				FMassEntityHandle Entity = Subsystem->FindEntityByNetId(FArcMassNetId(1));
				if (!Entity.IsSet()) return false;
				FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*ClientState.World);
				FMassEntityView EntityView(EntityManager, Entity);
				const FArcMassTestItemFragment* Frag = EntityView.GetFragmentDataPtr<FArcMassTestItemFragment>();
				return Frag && Frag->ReplicatedItems.Items.Num() == 2;
			})
			.ThenClients(TEXT("Verify updated items"), [this](FItemTestState& ClientState)
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				FMassEntityHandle Entity = Subsystem->FindEntityByNetId(FArcMassNetId(1));
				FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*ClientState.World);
				FMassEntityView EntityView(EntityManager, Entity);

				const FArcMassTestItemFragment* Frag =
					EntityView.GetFragmentDataPtr<FArcMassTestItemFragment>();
				ASSERT_THAT(IsNotNull(Frag));
				ASSERT_THAT(AreEqual(Frag->ReplicatedItems.Items.Num(), 2));
				ASSERT_THAT(AreEqual(Frag->ReplicatedItems.Items[0].ItemId, 1));
				ASSERT_THAT(AreEqual(Frag->ReplicatedItems.Items[0].Value, 999));
				ASSERT_THAT(AreEqual(Frag->ReplicatedItems.Items[1].ItemId, 2));
				ASSERT_THAT(AreEqual(Frag->ReplicatedItems.Items[1].Value, 777));
			});
	}
};

// ---------------------------------------------------------------------------
// Nested struct item replication — verifies FStructNetSerializer walks nested USTRUCTs
// ---------------------------------------------------------------------------

NETWORK_TEST_CLASS(ArcIrisNestedItemReplication, "ArcMassReplication.NestedItemArray")
{
	struct FNestedTestState : public FBasePIENetworkComponentState
	{
		AArcMassTestNestedItemProxy* Proxy = nullptr;
		FArcMassNetId AllocatedNetId;
	};

	FPIENetworkComponent<FNestedTestState> Network{ TestRunner, TestCommandBuilder, bInitializing };

	inline static TSharedPtr<FScopedTestEnvironment> TestEnvironment{ nullptr };
	BEFORE_ALL()
	{
		TestEnvironment = FScopedTestEnvironment::Get();
		TestEnvironment->SetConsoleVariableValue(TEXT("net.Iris.UseIrisReplication"), TEXT("1"));
	}

	BEFORE_EACH()
	{
		TestRunner->AddExpectedError(TEXT("FMassEntityQuery::CacheArchetypes"), EAutomationExpectedErrorFlags::Contains, -1);
		TestRunner->AddExpectedError(TEXT("SendRPC"), EAutomationExpectedErrorFlags::Contains, -1);

		FNetworkComponentBuilder<FNestedTestState>()
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

	TEST_METHOD(NestedStruct_ReplicatesToClient)
	{
		Network
			.ThenServer(TEXT("Spawn proxy with nested struct items"), [this](FNestedTestState& ServerState)
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ServerState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				ASSERT_THAT(IsNotNull(Subsystem));

				ServerState.Proxy = Subsystem->SpawnProxyOfClass<AArcMassTestNestedItemProxy>();
				ASSERT_THAT(IsNotNull(ServerState.Proxy));

				ServerState.AllocatedNetId = Subsystem->AllocateNetId();

				FArcMassTestNestedItemFragment Frag;

				FArcMassTestNestedItem Item1;
				Item1.ItemId = 1;
				Item1.NestedData.Position = FVector(100.0, 200.0, 300.0);
				Item1.NestedData.Health = 75.5f;
				Item1.NestedData.Tag = FName(TEXT("Warrior"));
				Frag.ReplicatedItems.AddItem(MoveTemp(Item1));

				FArcMassTestNestedItem Item2;
				Item2.ItemId = 2;
				Item2.NestedData.Position = FVector(-50.0, 0.0, 100.0);
				Item2.NestedData.Health = 100.0f;
				Item2.NestedData.Tag = FName(TEXT("Healer"));
				Frag.ReplicatedItems.AddItem(MoveTemp(Item2));

				TArray<FInstancedStruct> Slots;
				Slots.Add(FInstancedStruct::Make(Frag));
				ServerState.Proxy->SetEntityFragments(ServerState.AllocatedNetId, Slots);
			})
			.UntilClients(TEXT("Wait for entity on client"), [](FNestedTestState& ClientState) -> bool
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				if (!Subsystem) return false;
				FMassEntityHandle Entity = Subsystem->FindEntityByNetId(FArcMassNetId(1));
				return Entity.IsSet();
			})
			.ThenClients(TEXT("Verify nested struct fields replicated"), [this](FNestedTestState& ClientState)
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				FMassEntityHandle Entity = Subsystem->FindEntityByNetId(FArcMassNetId(1));
				FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*ClientState.World);
				FMassEntityView EntityView(EntityManager, Entity);

				const FArcMassTestNestedItemFragment* Frag =
					EntityView.GetFragmentDataPtr<FArcMassTestNestedItemFragment>();
				ASSERT_THAT(IsNotNull(Frag));
				ASSERT_THAT(AreEqual(Frag->ReplicatedItems.Items.Num(), 2));

				const FArcMassTestNestedItem& Item1 = Frag->ReplicatedItems.Items[0];
				ASSERT_THAT(AreEqual(Item1.ItemId, 1));
				ASSERT_THAT(IsNear(Item1.NestedData.Position.X, 100.0, 0.1));
				ASSERT_THAT(IsNear(Item1.NestedData.Position.Y, 200.0, 0.1));
				ASSERT_THAT(IsNear(Item1.NestedData.Position.Z, 300.0, 0.1));
				ASSERT_THAT(IsNear(Item1.NestedData.Health, 75.5f, 0.01f));
				ASSERT_THAT(AreEqual(Item1.NestedData.Tag, FName(TEXT("Warrior"))));

				const FArcMassTestNestedItem& Item2 = Frag->ReplicatedItems.Items[1];
				ASSERT_THAT(AreEqual(Item2.ItemId, 2));
				ASSERT_THAT(IsNear(Item2.NestedData.Health, 100.0f, 0.01f));
				ASSERT_THAT(AreEqual(Item2.NestedData.Tag, FName(TEXT("Healer"))));
			});
	}
};

// ---------------------------------------------------------------------------
// FMassEntityHandle item replication — verifies custom Iris net serializer on item field
// ---------------------------------------------------------------------------

NETWORK_TEST_CLASS(ArcIrisEntityHandleItemReplication, "ArcMassReplication.EntityHandleItemArray")
{
	struct FHandleItemTestState : public FBasePIENetworkComponentState
	{
		AArcMassTestEntityHandleItemProxy* Proxy = nullptr;
		FArcMassNetId AllocatedNetId;
		FMassEntityHandle ServerEntity;
	};

	FPIENetworkComponent<FHandleItemTestState> Network{ TestRunner, TestCommandBuilder, bInitializing };

	inline static TSharedPtr<FScopedTestEnvironment> TestEnvironment{ nullptr };
	BEFORE_ALL()
	{
		TestEnvironment = FScopedTestEnvironment::Get();
		TestEnvironment->SetConsoleVariableValue(TEXT("net.Iris.UseIrisReplication"), TEXT("1"));
	}

	BEFORE_EACH()
	{
		TestRunner->AddExpectedError(TEXT("FMassEntityQuery::CacheArchetypes"), EAutomationExpectedErrorFlags::Contains, -1);
		TestRunner->AddExpectedError(TEXT("SendRPC"), EAutomationExpectedErrorFlags::Contains, -1);

		FNetworkComponentBuilder<FHandleItemTestState>()
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

	TEST_METHOD(EntityHandleItem_ReplicatesToClient)
	{
		Network
			.ThenServer(TEXT("Spawn proxy with entity handle items"), [this](FHandleItemTestState& ServerState)
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ServerState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				ASSERT_THAT(IsNotNull(Subsystem));

				ServerState.Proxy = Subsystem->SpawnProxyOfClass<AArcMassTestEntityHandleItemProxy>();
				ASSERT_THAT(IsNotNull(ServerState.Proxy));

				ServerState.AllocatedNetId = Subsystem->AllocateNetId();

				FArcMassTestEntityHandleItemFragment Frag;

				FArcMassTestEntityHandleItem Item1;
				Item1.ItemId = 42;
				Item1.Priority = 5;
				Frag.ReplicatedItems.AddItem(MoveTemp(Item1));

				TArray<FInstancedStruct> Slots;
				Slots.Add(FInstancedStruct::Make(Frag));
				ServerState.Proxy->SetEntityFragments(ServerState.AllocatedNetId, Slots);
			})
			.UntilClients(TEXT("Wait for entity on client"), [](FHandleItemTestState& ClientState) -> bool
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				if (!Subsystem) return false;
				FMassEntityHandle Entity = Subsystem->FindEntityByNetId(FArcMassNetId(1));
				return Entity.IsSet();
			})
			.ThenClients(TEXT("Verify entity handle item replicated"), [this](FHandleItemTestState& ClientState)
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				FMassEntityHandle Entity = Subsystem->FindEntityByNetId(FArcMassNetId(1));
				FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*ClientState.World);
				FMassEntityView EntityView(EntityManager, Entity);

				const FArcMassTestEntityHandleItemFragment* Frag =
					EntityView.GetFragmentDataPtr<FArcMassTestEntityHandleItemFragment>();
				ASSERT_THAT(IsNotNull(Frag));
				ASSERT_THAT(AreEqual(Frag->ReplicatedItems.Items.Num(), 1));
				ASSERT_THAT(AreEqual(Frag->ReplicatedItems.Items[0].ItemId, 42));
				ASSERT_THAT(AreEqual(Frag->ReplicatedItems.Items[0].Priority, 5));
			});
	}
};

// ---------------------------------------------------------------------------
// Delta replication — only changed items replicate, verified via callback counters
// ---------------------------------------------------------------------------

NETWORK_TEST_CLASS(ArcIrisDeltaReplication, "ArcMassReplication.DeltaItemArray")
{
	struct FDeltaTestState : public FBasePIENetworkComponentState
	{
		AArcMassTestItemProxy* Proxy = nullptr;
		FArcMassNetId AllocatedNetId;
	};

	FPIENetworkComponent<FDeltaTestState> Network{ TestRunner, TestCommandBuilder, bInitializing };

	inline static TSharedPtr<FScopedTestEnvironment> TestEnvironment{ nullptr };
	BEFORE_ALL()
	{
		TestEnvironment = FScopedTestEnvironment::Get();
		TestEnvironment->SetConsoleVariableValue(TEXT("net.Iris.UseIrisReplication"), TEXT("1"));
	}

	BEFORE_EACH()
	{
		TestRunner->AddExpectedError(TEXT("FMassEntityQuery::CacheArchetypes"), EAutomationExpectedErrorFlags::Contains, -1);
		TestRunner->AddExpectedError(TEXT("SendRPC"), EAutomationExpectedErrorFlags::Contains, -1);

		FNetworkComponentBuilder<FDeltaTestState>()
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

	TEST_METHOD(DeltaReplication_OnlyChangedItemReplicates)
	{
		Network
			.ThenServer(TEXT("Add 3 items"), [this](FDeltaTestState& ServerState)
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ServerState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				ASSERT_THAT(IsNotNull(Subsystem));

				ServerState.Proxy = Subsystem->SpawnProxyOfClass<AArcMassTestItemProxy>();
				ASSERT_THAT(IsNotNull(ServerState.Proxy));

				ServerState.AllocatedNetId = Subsystem->AllocateNetId();

				FArcMassTestItemFragment Frag;

				FArcMassTestReplicatedItem Item1;
				Item1.ItemId = 1;
				Item1.Value = 100;
				Frag.ReplicatedItems.AddItem(MoveTemp(Item1));

				FArcMassTestReplicatedItem Item2;
				Item2.ItemId = 2;
				Item2.Value = 200;
				Frag.ReplicatedItems.AddItem(MoveTemp(Item2));

				FArcMassTestReplicatedItem Item3;
				Item3.ItemId = 3;
				Item3.Value = 300;
				Frag.ReplicatedItems.AddItem(MoveTemp(Item3));

				TArray<FInstancedStruct> Slots;
				Slots.Add(FInstancedStruct::Make(Frag));
				ServerState.Proxy->SetEntityFragments(ServerState.AllocatedNetId, Slots);
			})
			.UntilClients(TEXT("Wait for all 3 items on client"), [](FDeltaTestState& ClientState) -> bool
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				if (!Subsystem) return false;
				FMassEntityHandle Entity = Subsystem->FindEntityByNetId(FArcMassNetId(1));
				if (!Entity.IsSet()) return false;
				FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*ClientState.World);
				FMassEntityView EntityView(EntityManager, Entity);
				const FArcMassTestItemFragment* Frag = EntityView.GetFragmentDataPtr<FArcMassTestItemFragment>();
				return Frag && Frag->ReplicatedItems.Items.Num() == 3;
			})
			.ThenClients(TEXT("Log initial state and reset counters"), [this](FDeltaTestState& ClientState)
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				FMassEntityHandle Entity = Subsystem->FindEntityByNetId(FArcMassNetId(1));
				FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*ClientState.World);
				FMassEntityView EntityView(EntityManager, Entity);
				const FArcMassTestItemFragment* Frag = EntityView.GetFragmentDataPtr<FArcMassTestItemFragment>();
				ASSERT_THAT(IsNotNull(Frag));

				UE_LOG(LogTemp, Warning, TEXT("Delta test - initial state: Items=%d AddCb=%d ChangeCb=%d RemoveCb=%d"),
					Frag->ReplicatedItems.Items.Num(),
					Frag->ReplicatedItems.AddCallbackCount,
					Frag->ReplicatedItems.ChangeCallbackCount,
					Frag->ReplicatedItems.RemoveCallbackCount);

				ASSERT_THAT(AreEqual(Frag->ReplicatedItems.AddCallbackCount, 3));
				ASSERT_THAT(AreEqual(Frag->ReplicatedItems.ChangeCallbackCount, 0));
			})
			.ThenServer(TEXT("Modify only item 2"), [this](FDeltaTestState& ServerState)
			{
				FArcMassTestItemFragment Frag;

				FArcMassTestReplicatedItem Item1;
				Item1.ItemId = 1;
				Item1.Value = 100;
				Frag.ReplicatedItems.AddItem(MoveTemp(Item1));

				FArcMassTestReplicatedItem Item2;
				Item2.ItemId = 2;
				Item2.Value = 999;
				Frag.ReplicatedItems.AddItem(MoveTemp(Item2));

				FArcMassTestReplicatedItem Item3;
				Item3.ItemId = 3;
				Item3.Value = 300;
				Frag.ReplicatedItems.AddItem(MoveTemp(Item3));

				TArray<FInstancedStruct> Slots;
				Slots.Add(FInstancedStruct::Make(Frag));
				ServerState.Proxy->SetEntityFragments(ServerState.AllocatedNetId, Slots);
			})
			.UntilClients(TEXT("Wait for item 2 update"), [](FDeltaTestState& ClientState) -> bool
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				if (!Subsystem) return false;
				FMassEntityHandle Entity = Subsystem->FindEntityByNetId(FArcMassNetId(1));
				if (!Entity.IsSet()) return false;
				FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*ClientState.World);
				FMassEntityView EntityView(EntityManager, Entity);
				const FArcMassTestItemFragment* Frag = EntityView.GetFragmentDataPtr<FArcMassTestItemFragment>();
				if (!Frag) return false;
				for (const FArcMassTestReplicatedItem& Item : Frag->ReplicatedItems.Items)
				{
					if (Item.ItemId == 2 && Item.Value == 999) return true;
				}
				return false;
			})
			.ThenClients(TEXT("Verify only 1 change callback, no extra adds"), [this](FDeltaTestState& ClientState)
			{
				UArcMassEntityReplicationSubsystem* Subsystem =
					ClientState.World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
				FMassEntityHandle Entity = Subsystem->FindEntityByNetId(FArcMassNetId(1));
				FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*ClientState.World);
				FMassEntityView EntityView(EntityManager, Entity);

				const FArcMassTestItemFragment* Frag =
					EntityView.GetFragmentDataPtr<FArcMassTestItemFragment>();
				ASSERT_THAT(IsNotNull(Frag));
				ASSERT_THAT(AreEqual(Frag->ReplicatedItems.Items.Num(), 3));

				UE_LOG(LogTemp, Warning, TEXT("Delta test - after update: Items=%d AddCb=%d ChangeCb=%d RemoveCb=%d"),
					Frag->ReplicatedItems.Items.Num(),
					Frag->ReplicatedItems.AddCallbackCount,
					Frag->ReplicatedItems.ChangeCallbackCount,
					Frag->ReplicatedItems.RemoveCallbackCount);

				ASSERT_THAT(AreEqual(Frag->ReplicatedItems.ChangeCallbackCount, 1));
				ASSERT_THAT(AreEqual(Frag->ReplicatedItems.AddCallbackCount, 3));
			});
	}
};

// ---------------------------------------------------------------------------
// Direct actor UPROPERTY — tests Apply fires for top-level replicated array
// ---------------------------------------------------------------------------

NETWORK_TEST_CLASS(ArcIrisDirectArrayReplication, "ArcMassReplication.DirectArray")
{
	struct FDirectTestState : public FBasePIENetworkComponentState
	{
		AArcMassTestDirectArrayActor* Actor = nullptr;
	};

	FPIENetworkComponent<FDirectTestState> Network{ TestRunner, TestCommandBuilder, bInitializing };

	inline static TSharedPtr<FScopedTestEnvironment> TestEnvironment{ nullptr };
	BEFORE_ALL()
	{
		TestEnvironment = FScopedTestEnvironment::Get();
		TestEnvironment->SetConsoleVariableValue(TEXT("net.Iris.UseIrisReplication"), TEXT("1"));
	}

	BEFORE_EACH()
	{
		TestRunner->AddExpectedError(TEXT("FMassEntityQuery::CacheArchetypes"), EAutomationExpectedErrorFlags::Contains, -1);
		TestRunner->AddExpectedError(TEXT("SendRPC"), EAutomationExpectedErrorFlags::Contains, -1);

		FNetworkComponentBuilder<FDirectTestState>()
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

	TEST_METHOD(DirectProperty_ReplicatesWithCallbacks)
	{
		Network
			.ThenServer(TEXT("Spawn actor, add 2 items"), [this](FDirectTestState& ServerState)
			{
				ServerState.Actor = ServerState.World->SpawnActor<AArcMassTestDirectArrayActor>();
				ASSERT_THAT(IsNotNull(ServerState.Actor));

				FArcMassTestReplicatedItem Item1;
				Item1.ItemId = 10;
				Item1.Value = 100;
				ServerState.Actor->ReplicatedItems.AddItem(MoveTemp(Item1));

				FArcMassTestReplicatedItem Item2;
				Item2.ItemId = 20;
				Item2.Value = 200;
				ServerState.Actor->ReplicatedItems.AddItem(MoveTemp(Item2));
			})
			.UntilClients(TEXT("Wait for actor on client with items"), [](FDirectTestState& ClientState) -> bool
			{
				for (TActorIterator<AArcMassTestDirectArrayActor> It(ClientState.World); It; ++It)
				{
					if (It->ReplicatedItems.Items.Num() == 2)
					{
						ClientState.Actor = *It;
						return true;
					}
				}
				return false;
			})
			.ThenClients(TEXT("Verify items and callbacks"), [this](FDirectTestState& ClientState)
			{
				ASSERT_THAT(IsNotNull(ClientState.Actor));
				ASSERT_THAT(AreEqual(ClientState.Actor->ReplicatedItems.Items.Num(), 2));
				ASSERT_THAT(AreEqual(ClientState.Actor->ReplicatedItems.Items[0].ItemId, 10));
				ASSERT_THAT(AreEqual(ClientState.Actor->ReplicatedItems.Items[0].Value, 100));
				ASSERT_THAT(AreEqual(ClientState.Actor->ReplicatedItems.Items[1].ItemId, 20));
				ASSERT_THAT(AreEqual(ClientState.Actor->ReplicatedItems.Items[1].Value, 200));

				UE_LOG(LogTemp, Warning, TEXT("DirectArray test: Items=%d AddCb=%d ChangeCb=%d RemoveCb=%d"),
					ClientState.Actor->ReplicatedItems.Items.Num(),
					ClientState.Actor->ReplicatedItems.AddCallbackCount,
					ClientState.Actor->ReplicatedItems.ChangeCallbackCount,
					ClientState.Actor->ReplicatedItems.RemoveCallbackCount);

				ASSERT_THAT(AreEqual(ClientState.Actor->ReplicatedItems.AddCallbackCount, 2));
				ASSERT_THAT(AreEqual(ClientState.Actor->ReplicatedItems.ChangeCallbackCount, 0));
			});
	}
};

#endif // ENABLE_PIE_NETWORK_TEST && UE_WITH_IRIS
