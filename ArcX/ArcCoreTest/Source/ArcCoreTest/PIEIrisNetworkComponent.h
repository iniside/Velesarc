/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#pragma once
#include "Commands/TestCommandBuilder.h"
#include "Components/PIENetworkTestStateRestorer.h"

#include "Iris/ReplicationSystem/NetRefHandle.h"
#include "Net/Core/NetHandle/NetHandleManager.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Net/Iris/ReplicationSystem/ReplicationSystemUtil.h"
#include "Net/Iris/ReplicationSystem/EngineReplicationBridge.h"
#include "GameFramework/GameState.h"

/**
 * Struct which handles the PIE session's world and network information.
 * Both the server and client will keep a reference to their own respective state which will be tested against.
 */
struct FBasePIEIrisNetworkComponentState
{
	/** Reference to the session's world. */
	UWorld* World = nullptr;

	/** Used by the server to reference the connections to the client. */
	TArray<UNetConnection*> ClientConnections;

	/** Used by the client to reference location within the `ClientConnections` array on the server. */
	int32 ClientIndex = INDEX_NONE;

	/** Used by the server for creating and validation of client instances. */
	int32 ClientCount = 2;

	/** Used by the server to create a PIE session as a dedicated or listen server. */
	bool bIsDedicatedServer = true;

	/** Used to track spawned and replicated actors across client and server PIE sessions. */
	TSet<FNetworkGUID> LocallySpawnedActors{};
};

/**
 * Component which creates and initializes the server and client network connections between the PIE sessions.
 * The component also provides methods around the CommandBuilder for adding latent commands.
 */
class ARCCORETEST_API FBasePIEIrisNetworkComponent
{
public:
	/**
	 * Construct the BasePIENetworkComponent.
	 *
	 * @param InTestRunner - Pointer to the TestRunner used for test reporting.
	 * @param InCommandBuilder - Reference to the latent command manager.
	 * @param IsInitializing - Bool specifying if the Automation framework is still being initialized.
	 * 
	 * @note requires that the `FBasePIEIrisNetworkComponent` is built using the `FIrisNetworkComponentBuilder` as this will setup the server and client `FBasePIEIrisNetworkComponentState`
	 */
	FBasePIEIrisNetworkComponent(FAutomationTestBase* InTestRunner, FTestCommandBuilder& InCommandBuilder, bool IsInitializing);

	/**
	 * Enqueues a function to the latent command manager for execution on a subsequent frame.
	 * 
	 * @param Action - Function to be executed.
	 * 
	 * @return a reference to this
	 */
	FBasePIEIrisNetworkComponent& Then(TFunction<void()> Action);

	/**
	 * Enqueues a function to the latent command manager for execution on a subsequent frame.
	 *
	 * @param Description - Description of the provided function to be logged.
	 * @param Action - Function to be executed.
	 * 
	 * @return a reference to this
	 */
	FBasePIEIrisNetworkComponent& Then(const TCHAR* Description, TFunction<void()> Action);

	/**
	 * Enqueues a function to the latent command manager for execution on a subsequent frame.
	 *
	 * @param Action - Function to be executed.
	 * 
	 * @return a reference to this
	 */
	FBasePIEIrisNetworkComponent& Do(TFunction<void()> Action);

	/**
	 * Enqueues a function to the latent command manager for execution on a subsequent frame.
	 *
	 * @param Description - Description of the provided function to be logged.
	 * @param Action - Function to be executed.
	 * 
	 * @return a reference to this
	 */
	FBasePIEIrisNetworkComponent& Do(const TCHAR* Description, TFunction<void()> Action);

	/**
	 * Enqueues a function to the latent command manager for execution until completion or timeout.
	 *
	 * @param Query - Function to be executed.
	 * @param Timeout - Duration that the latent command can execute before reporting an error.
	 * 
	 * @return a reference to this
	 * 
	 * @note Will continue to execute until the function provided evaluates to `true` or the duration exceeds the specified timeout.
	 */
	FBasePIEIrisNetworkComponent& Until(TFunction<bool()> Query, FTimespan Timeout = FTimespan::FromSeconds(10));

	/**
	 * Enqueues a function to the latent command manager for execution until completion or timeout.
	 *
	 * @param Description - Description of the provided function to be logged.
	 * @param Query - Function to be executed.
	 * @param Timeout - Duration that the latent command can execute before reporting an error.
	 * 
	 * @return a reference to this
	 *
	 * @note Will continue to execute until the function provided evaluates to `true` or the duration exceeds the specified timeout.
	 */
	FBasePIEIrisNetworkComponent& Until(const TCHAR* Description, TFunction<bool()> Query, FTimespan Timeout = FTimespan::FromSeconds(10));
	
	/**
	 * Enqueues a function to the latent command manager for execution until completion or timeout.
	 *
	 * @param Query - Function to be executed.
	 * @param Timeout - Duration that the latent command can execute before reporting an error.
	 * 
	 * @return a reference to this
	 *
	 * @note Will continue to execute until the function provided evaluates to `true` or the duration exceeds the specified timeout.
	 */
	FBasePIEIrisNetworkComponent& StartWhen(TFunction<bool()> Query, FTimespan Timeout = FTimespan::FromSeconds(10));

	/**
	 * Enqueues a function to the latent command manager for execution until completion or timeout.
	 *
	 * @param Description - Description of the provided function to be logged.
	 * @param Query - Function to be executed.
	 * @param Timeout - Duration that the latent command can execute before reporting an error.
	 * 
	 * @return a reference to this
	 *
	 * @note Will continue to execute until the function provided evaluates to `true` or the duration exceeds the specified timeout.
	 */
	FBasePIEIrisNetworkComponent& StartWhen(const TCHAR* Description, TFunction<bool()> Query, FTimespan Timeout = FTimespan::FromSeconds(10));

protected:
	/** Prepares the active PIE sessions to be stopped. */
	void StopPie();

	/** Setup settings for a network session and start PIE sessions for both the server and client with the network settings applied. */
	void StartPie();
	
	/**
	 * Fetch all of the worlds that will be used for the networked session.
	 *
	 * @return true if an error was encountered or if the PIE sessions were created with the correct network settings, false otherwise.
	 *
	 * @note Method is expected to be used within the `Until` latent command to then wait until the worlds are ready for use.
	 */
	bool SetWorlds();

	/** Sets the packet settings used to simulate packet behavior over a network. */
	void SetPacketSettings() const;

	/** Connects all available clients to the server. */
	void ConnectClientsToServer();

	/**
	 * Go through all of the client connections to make sure they are connected and ready.
	 *
	 * @return true if an error was encountered or if the connections all have a valid controller, false otherwise.
	 *
	 * @note Method is expected to be used within the `Until` latent command to then wait until the worlds are ready for use.
	 */
	bool AwaitClientsReady() const;

	/** Restore back to the state prior to test execution. */
	void RestoreState();

	TUniquePtr<FBasePIEIrisNetworkComponentState> ServerState = nullptr;
	TArray<TUniquePtr<FBasePIEIrisNetworkComponentState>> ClientStates;
	FAutomationTestBase* TestRunner = nullptr;
	FTestCommandBuilder* CommandBuilder = nullptr;
	FPacketSimulationSettings* PacketSimulationSettings = nullptr;
	TSubclassOf<AGameModeBase> GameMode = TSubclassOf<AGameModeBase>(nullptr);
	FPIENetworkTestStateRestorer StateRestorer;
	TMap<FNetworkGUID, int64> SpawnedActors{};
};

template <typename NetworkDataType>
class FIrisNetworkComponentBuilder;

/**
 * Component which creates and initializes the server and client network connections between the PIE sessions.
 * Expands on the `FBasePIEIrisNetworkComponent` by providing separate methods to add latent commands for the server and clients.
 */
template <typename NetworkDataType = FBasePIEIrisNetworkComponentState>
class FPIEIrisNetworkComponent : public FBasePIEIrisNetworkComponent
{
public:
	/**
	 * Construct the PIENetworkComponent.
	 *
	 * @param InTestRunner - Pointer to the TestRunner used for test reporting.
	 * @param InCommandBuilder - Reference to the latent command manager.
	 * @param IsInitializing - Bool specifying if the Automation framework is still being initialized.
	 *
	 * @note Requires that the `FPIEIrisNetworkComponent` is built using the `FIrisNetworkComponentBuilder` as this will setup the server and client `NetworkDataType` states
	 */
	FPIEIrisNetworkComponent(FAutomationTestBase* InTestRunner, FTestCommandBuilder& InCommandBuilder, bool IsInitializing)
		: FBasePIEIrisNetworkComponent(InTestRunner, InCommandBuilder, IsInitializing) {}

	/**
	 * Enqueues a function to the latent command manager for execution on a subsequent frame on the server.
	 *
	 * @param Action - Function to be executed.
	 * 
	 * @return a reference to this
	 */
	FPIEIrisNetworkComponent& ThenServer(TFunction<void(NetworkDataType&)> Action);

	/**
	 * Enqueues a function to the latent command manager for execution on a subsequent frame on the server.
	 *
	 * @param Description - Description of the provided function to be logged.
	 * @param Action - Function to be executed.
	 * 
	 * @return a reference to this
	 */
	FPIEIrisNetworkComponent& ThenServer(const TCHAR* Description, TFunction<void(NetworkDataType&)> Action);

	/**
	 * Enqueues a function to the latent command manager for execution on a subsequent frame on all clients.
	 *
	 * @param Action - Function to be executed.
	 * 
	 * @return a reference to this
	 */
	FPIEIrisNetworkComponent& ThenClients(TFunction<void(NetworkDataType&)> Action);

	/**
	 * Enqueues a function to the latent command manager for execution on a subsequent frame on all clients.
	 *
	 * @param Description - Description of the provided function to be logged.
	 * @param Action - Function to be executed.
	 * 
	 * @return a reference to this
	 */
	FPIEIrisNetworkComponent& ThenClients(const TCHAR* Description, TFunction<void(NetworkDataType&)> Action);

	/**
	 * Enqueues a function to the latent command manager for execution on a subsequent frame on a given client.
	 *
	 * @param ClientIndex - Index of the connected client.
	 * @param Action - Function to be executed.
	 * 
	 * @return a reference to this
	 */
	FPIEIrisNetworkComponent& ThenClient(int32 ClientIndex, TFunction<void(NetworkDataType&)> Action);

	/**
	 * Enqueues a function to the latent command manager for execution on a subsequent frame on a given client.
	 *
	 * @param Description - Description of the provided function to be logged.
	 * @param ClientIndex - Index of the connected client.
	 * @param Action - Function to be executed.
	 * 
	 * @return a reference to this
	 */
	FPIEIrisNetworkComponent& ThenClient(const TCHAR* Description, int32 ClientIndex, TFunction<void(NetworkDataType&)> Action);

	/**
	 * Enqueues a function to the latent command manager for execution until completion or timeout on the server.
	 *
	 * @param Query - Function to be executed.
	 * @param Timeout - Duration that the latent command can execute before reporting an error.
	 * 
	 * @return a reference to this
	 */
	FPIEIrisNetworkComponent& UntilServer(TFunction<bool(NetworkDataType&)> Query, FTimespan Timeout = FTimespan::FromSeconds(10));

	/**
	 * Enqueues a function to the latent command manager for execution until completion or timeout on the server.
	 *
	 * @param Description - Description of the latent command
	 * @param Query - Function to be executed.
	 * @param Timeout - Duration that the latent command can execute before reporting an error.
	 * 
	 * @return a reference to this
	 */
	FPIEIrisNetworkComponent& UntilServer(const TCHAR* Description, TFunction<bool(NetworkDataType&)> Query, FTimespan Timeout = FTimespan::FromSeconds(10));

	/**
	 * Enqueues a function to the latent command manager for execution until completion or timeout on all clients.
	 *
	 * @param Query - Function to be executed.
	 * @param Timeout - Duration that the latent command can execute before reporting an error.
	 * 
	 * @return a reference to this
	 */
	FPIEIrisNetworkComponent& UntilClients(TFunction<bool(NetworkDataType&)> Query, FTimespan Timeout = FTimespan::FromSeconds(10));

	/**
	 * Enqueues a function to the latent command manager for execution until completion or timeout on all clients.
	 *
	 * @param Description - Description of the latent command
	 * @param Query - Function to be executed.
	 * @param Timeout - Duration that the latent command can execute before reporting an error.
	 * 
	 * @return a reference to this
	 */
	FPIEIrisNetworkComponent& UntilClients(const TCHAR* Description, TFunction<bool(NetworkDataType&)> Query, FTimespan Timeout = FTimespan::FromSeconds(10));
	
	/**
	 * Enqueues a function to the latent command manager for execution until completion or timeout on a given client.
	 *
	 * @param ClientIndex - Index of the connected client.
	 * @param Query - Function to be executed.
	 * @param Timeout - Duration that the latent command can execute before reporting an error.
	 * 
	 * @return a reference to this
	 */
	FPIEIrisNetworkComponent& UntilClient(int32 ClientIndex, TFunction<bool(NetworkDataType&)> Query, FTimespan Timeout = FTimespan::FromSeconds(10));

	/**
	 * Enqueues a function to the latent command manager for execution until completion or timeout on a given client.
	 *
	 * @param Description - Description of the latent command
	 * @param ClientIndex - Index of the connected client.
	 * @param Query - Function to be executed.
	 * @param Timeout - Duration that the latent command can execute before reporting an error.
	 * 
	 * @return a reference to this
	 */
	FPIEIrisNetworkComponent& UntilClient(const TCHAR* Description, int32 ClientIndex, TFunction<bool(NetworkDataType&)> Query, FTimespan Timeout = FTimespan::FromSeconds(10));

	/**
	 * Has a new client request a late join and syncs the server and other clients with the new client PIE session.
	 *
	 * @return a reference to this
	 */
	FPIEIrisNetworkComponent& ThenClientJoins();
	
	/**
	 * Spawns an actor on the server and replicates to the clients.
	 *
	 * @return a reference to this
	 */
	template<typename ActorToSpawn, ActorToSpawn* NetworkDataType::*ResultStorage>
	FPIEIrisNetworkComponent& SpawnAndReplicate(bool bSetOwner = false);

	template<typename ActorToSpawn, ActorToSpawn* NetworkDataType::*ResultStorage>
	FPIEIrisNetworkComponent& SpawnAndReplicate(const FActorSpawnParameters& SpawnParameters);

	template<typename ActorToSpawn, ActorToSpawn* NetworkDataType::* ResultStorage>
	FPIEIrisNetworkComponent& SpawnAndReplicate(TFunction<void(ActorToSpawn&)> BeforeReplicate);

	template<typename ActorToSpawn, ActorToSpawn* NetworkDataType::* ResultStorage>
	FPIEIrisNetworkComponent& SpawnAndReplicate(const FActorSpawnParameters& SpawnParameters, TFunction<void(ActorToSpawn&)> BeforeReplicate);

private:
	friend class FIrisNetworkComponentBuilder<NetworkDataType>;
	template<typename ActorToSpawn, ActorToSpawn* NetworkDataType::* ResultStorage>
	FPIEIrisNetworkComponent& SpawnOnServer(const FActorSpawnParameters& SpawnParameters, TFunction<void(ActorToSpawn&)> BeforeReplicate);

	bool ReplicateToClients(NetworkDataType& ClientState);
};


/**
 * Helper object used to setup and build the `FPIEIrisNetworkComponent`.
 */
template <typename NetworkDataType = FBasePIEIrisNetworkComponentState>
class FIrisNetworkComponentBuilder
{
public:
	/**
	 * Construct the NetworkComponentBuilder.
	 *
	 * @note Will assert if the `NetworkDataType` used is not of type or derived from `FBasePIEIrisNetworkComponentState`
	 */
	FIrisNetworkComponentBuilder();

	/**
	 * Specifies the number of clients to be used.
	 *
	 * @param ClientCount - Number of clients not including the listen server.
	 * 
	 * @return a reference to this
	 * 
	 * @note Number of clients should exclude the server, even if using the server as a listen server.
	 */
	FIrisNetworkComponentBuilder& WithClients(int32 ClientCount);

	/**
	 * Build the server as a dedicated server.
	 * 
	 * @return a reference to this
	 */
	FIrisNetworkComponentBuilder& AsDedicatedServer();

	/**
	 * Build the server as a listen server.
	 *
	 * @return a reference to this
	 */
	FIrisNetworkComponentBuilder& AsListenServer();

	/**
	 * Build the FPIEIrisNetworkComponent to use the provided packet simulation settings.
	 *
	 * @param InPacketSimulationSettings - Settings used to simulate packet behavior over a network.
	 * 
	 * @return a reference to this
	 */
	FIrisNetworkComponentBuilder& WithPacketSimulationSettings(FPacketSimulationSettings* InPacketSimulationSettings);

	/**
	 * Build the FPIEIrisNetworkComponent to use the provided game mode.
	 *
	 * @param InGameMode - GameMode used for the networked session.
	 * 
	 * @return a reference to this
	 */
	FIrisNetworkComponentBuilder& WithGameMode(TSubclassOf<AGameModeBase> InGameMode);

	/**
	 * Build the FPIEIrisNetworkComponent to restore back to the provided game instance.
	 *
	 * @param InGameInstanceClass - GameInstance to restore back to.
	 * 
	 * @return a reference to this
	 */
	FIrisNetworkComponentBuilder& WithGameInstanceClass(FSoftClassPath InGameInstanceClass);

	/**
	 * Build the FPIEIrisNetworkComponent with the provided data.
	 *
	 * @param OutNetwork - Reference to the FPIEIrisNetworkComponent.
	 * 
	 * @return a reference to this
	 */
	void Build(FPIEIrisNetworkComponent<NetworkDataType>& OutNetwork);

private:
	FPacketSimulationSettings* PacketSimulationSettings = nullptr;
	TSubclassOf<AGameModeBase> GameMode = TSubclassOf<AGameModeBase>(nullptr);
	FSoftClassPath GameInstanceClass = FSoftClassPath{};
	int32 ClientCount = 2;
	bool bIsDedicatedServer = true;
};


template <typename NetworkDataType>
inline FPIEIrisNetworkComponent<NetworkDataType>& FPIEIrisNetworkComponent<NetworkDataType>::ThenServer(TFunction<void(NetworkDataType&)> Action)
{
	CommandBuilder->Do([this, Action]() { Action(static_cast<NetworkDataType&>(*ServerState)); });		
	return *this;
}
template <typename NetworkDataType>
inline FPIEIrisNetworkComponent<NetworkDataType>& FPIEIrisNetworkComponent<NetworkDataType>::ThenServer(const TCHAR* Description, TFunction<void(NetworkDataType&)> Action)
{
	CommandBuilder->Do(Description, [this, Action] { Action(static_cast<NetworkDataType&>(*ServerState)); });
	return *this;
}

template <typename NetworkDataType>
inline FPIEIrisNetworkComponent<NetworkDataType>& FPIEIrisNetworkComponent<NetworkDataType>::ThenClients(TFunction<void(NetworkDataType&)> Action)
{
	for (int32 Index = 0; Index < ClientStates.Num(); Index++)
	{
		CommandBuilder->Do([this, Action, Index]() { Action(static_cast<NetworkDataType&>(*ClientStates[Index])); });
	}
	return *this;
}
template <typename NetworkDataType>
inline FPIEIrisNetworkComponent<NetworkDataType>& FPIEIrisNetworkComponent<NetworkDataType>::ThenClients(const TCHAR* Description, TFunction<void(NetworkDataType&)> Action)
{
	for (int32 Index = 0; Index < ClientStates.Num(); Index++)
	{
		CommandBuilder->Do(Description, [this, Action, Index]() { Action(static_cast<NetworkDataType&>(*ClientStates[Index])); });
	}
	return *this;
}

template <typename NetworkDataType>
inline FPIEIrisNetworkComponent<NetworkDataType>& FPIEIrisNetworkComponent<NetworkDataType>::ThenClient(int32 ClientIndex, TFunction<void(NetworkDataType&)> Action)
{
	if (!ClientStates.IsValidIndex(ClientIndex))
	{
		TestRunner->AddError(FString::Printf(TEXT("Invalid client index specified.  Requested Index %d out of %d"), ClientIndex, ClientStates.Num()));
		return *this;
	}

	CommandBuilder->Do([this, Action, ClientIndex]() { Action(static_cast<NetworkDataType&>(*ClientStates[ClientIndex])); });
	return *this;
}
template <typename NetworkDataType>
inline FPIEIrisNetworkComponent<NetworkDataType>& FPIEIrisNetworkComponent<NetworkDataType>::ThenClient(const TCHAR* Description, int32 ClientIndex, TFunction<void(NetworkDataType&)> Action)
{
	if (!ClientStates.IsValidIndex(ClientIndex))
	{
		TestRunner->AddError(FString::Printf(TEXT("Invalid client index specified.  Requested Index %d out of %d"), ClientIndex, ClientStates.Num()));
		return *this;
	}

	CommandBuilder->Do(Description, [this, Action, ClientIndex]() { Action(static_cast<NetworkDataType&>(*ClientStates[ClientIndex])); });
	return *this;
}

template <typename NetworkDataType>
inline FPIEIrisNetworkComponent<NetworkDataType>& FPIEIrisNetworkComponent<NetworkDataType>::UntilServer(TFunction<bool(NetworkDataType&)> Query, FTimespan Timeout)
{
	CommandBuilder->Until(
		[this, Query]() { return Query(static_cast<NetworkDataType&>(*ServerState)); }, Timeout);
	return *this;
}
template <typename NetworkDataType>
inline FPIEIrisNetworkComponent<NetworkDataType>& FPIEIrisNetworkComponent<NetworkDataType>::UntilServer(const TCHAR* Description, TFunction<bool(NetworkDataType&)> Query, FTimespan Timeout)
{
	CommandBuilder->Until(
		Description, [this, Query]() { return Query(static_cast<NetworkDataType&>(*ServerState)); }, Timeout);
	return *this;
}

template <typename NetworkDataType>
inline FPIEIrisNetworkComponent<NetworkDataType>& FPIEIrisNetworkComponent<NetworkDataType>::UntilClients(TFunction<bool(NetworkDataType&)> Query, FTimespan Timeout)
{
	for (int32 Index = 0; Index < ClientStates.Num(); Index++)
	{
		CommandBuilder->Until(
			[this, Query, Index]() { return Query(static_cast<NetworkDataType&>(*ClientStates[Index])); }, Timeout);
	}
	return *this;
}
template <typename NetworkDataType>
inline FPIEIrisNetworkComponent<NetworkDataType>& FPIEIrisNetworkComponent<NetworkDataType>::UntilClients(const TCHAR* Description, TFunction<bool(NetworkDataType&)> Query, FTimespan Timeout)
{
	for (int32 Index = 0; Index < ClientStates.Num(); Index++)
	{
		CommandBuilder->Until(
			Description, [this, Query, Index]() { return Query(static_cast<NetworkDataType&>(*ClientStates[Index])); }, Timeout);
	}
	return *this;
}
template <typename NetworkDataType>
inline FPIEIrisNetworkComponent<NetworkDataType>& FPIEIrisNetworkComponent<NetworkDataType>::UntilClient(int32 ClientIndex, TFunction<bool(NetworkDataType&)> Query, FTimespan Timeout)
{
	if (!ClientStates.IsValidIndex(ClientIndex))
	{
		TestRunner->AddError(FString::Printf(TEXT("Invalid client index specified.  Requested Index %d out of %d"), ClientIndex, ClientStates.Num()));
		return *this;
	}

	CommandBuilder->Until(
		[this, Query, ClientIndex]() { return Query(static_cast<NetworkDataType&>(*ClientStates[ClientIndex])); }, Timeout);
	return *this;
}
template <typename NetworkDataType>
inline FPIEIrisNetworkComponent<NetworkDataType>& FPIEIrisNetworkComponent<NetworkDataType>::UntilClient(const TCHAR* Description, int32 ClientIndex, TFunction<bool(NetworkDataType&)> Query, FTimespan Timeout)
{
	if (!ClientStates.IsValidIndex(ClientIndex))
	{
		TestRunner->AddError(FString::Printf(TEXT("Invalid client index specified.  Requested Index %d out of %d"), ClientIndex, ClientStates.Num()));
		return *this;
	}

	CommandBuilder->Until(
		Description, [this, Query, ClientIndex]() { return Query(static_cast<NetworkDataType&>(*ClientStates[ClientIndex])); }, Timeout);
	return *this;
}
#if UE_WITH_IRIS
template <typename NetworkDataType>
template <typename ActorToSpawn, ActorToSpawn* NetworkDataType::*ResultStorage>
inline FPIEIrisNetworkComponent<NetworkDataType>& FPIEIrisNetworkComponent<NetworkDataType>::SpawnAndReplicate(bool bSetOwner)
{
	static_assert(std::is_convertible_v<ActorToSpawn*, AActor*>, "ActorToSpawn must derive from AActor");

	TSharedPtr<ActorToSpawn*> SharedStorage = MakeShareable(new ActorToSpawn*(nullptr));
	TSharedRef<UE::Net::FNetRefHandle > NetRefHandle = MakeShareable(new UE::Net::FNetRefHandle ());
	ThenServer(TEXT("Spawning Actor On Server"), [SharedStorage, bSetOwner](NetworkDataType& State) {
		FActorSpawnParameters Params;
		if (bSetOwner)
		{
			Params.Owner = State.World->GetFirstPlayerController();	
		}
		
		*SharedStorage = State.World->template SpawnActor<ActorToSpawn>(Params);
		if (ResultStorage != nullptr)
		{
			State.*ResultStorage = *SharedStorage;
		}
	}).UntilServer(TEXT("Waiting for NetGUID"), [SharedStorage,  NetRefHandle](NetworkDataType& State)
		{
		ActorToSpawn* A = Cast<ActorToSpawn>(*SharedStorage.Get());
		UE::Net::FNetHandle Handle = UE::Net::FReplicationSystemUtil::GetNetHandle(A);
		if (const UEngineReplicationBridge* Bridge = UE::Net::FReplicationSystemUtil::GetActorReplicationBridge(State.World->GetGameState()))
		{
			*NetRefHandle = Bridge->GetReplicatedRefHandle(Handle);
		}
		return NetRefHandle->IsValid();

		}
	).UntilClients(TEXT("Waiting for Replication on Clients"), [NetRefHandle](NetworkDataType& State)
		{
		if (const UEngineReplicationBridge* Bridge = UE::Net::FReplicationSystemUtil::GetActorReplicationBridge(State.World->GetGameState()))
		{
			ActorToSpawn* A = Cast<ActorToSpawn>(Bridge->GetReplicatedObject(*NetRefHandle));
			if (A == nullptr)
			{
				return false;
			}
			
			if (ResultStorage != nullptr)
			{
				State.*ResultStorage = A;
			}
			return true;
		}
		return false;
	});

	return *this;
}

#else
template <typename NetworkDataType>
template <typename ActorToSpawn, ActorToSpawn* NetworkDataType::*ResultStorage>
inline FPIEIrisNetworkComponent<NetworkDataType>& FPIEIrisNetworkComponent<NetworkDataType>::SpawnAndReplicate(bool bSetOwner)
{
	static_assert(std::is_convertible_v<ActorToSpawn*, AActor*>, "ActorToSpawn must derive from AActor");

	TSharedPtr<ActorToSpawn*> SharedStorage = MakeShareable(new ActorToSpawn*(nullptr));
	TSharedRef<FNetworkGUID> SharedGuid = MakeShareable(new FNetworkGUID());
	ThenServer(TEXT("Spawning Actor On Server"), [SharedStorage](NetworkDataType& State) {
		FActorSpawnParameters Params;
		Params.Owner = State.World->GetFirstPlayerController();
		*SharedStorage = State.World->template SpawnActor<ActorToSpawn>(Params);
		if (ResultStorage != nullptr)
		{
			State.*ResultStorage = *SharedStorage;
		}
	}).UntilServer(TEXT("Waiting for NetGUID"), [SharedStorage, SharedGuid](NetworkDataType& State)
		{
		*SharedGuid = State.World->GetNetDriver()->GuidCache->GetNetGUID(*SharedStorage);
			return SharedGuid->IsValid();
		}
	).UntilClients(TEXT("Waiting for Replication on Clients"), [SharedGuid](NetworkDataType& State)
		{
		ActorToSpawn* ClientActor = Cast<ActorToSpawn>(State.World->GetNetDriver()->GuidCache->GetObjectFromNetGUID(*SharedGuid, true));
				if (ClientActor == nullptr)
				{
					return false;
				}
				if (ResultStorage != nullptr)
				{
					State.*ResultStorage = ClientActor;
				}
				return true;
	});

	return *this;
}
#endif

///////////////////////////////////////////////////////////////////////
// FIrisNetworkComponentBuilder

template <typename NetworkDataType>
inline FIrisNetworkComponentBuilder<NetworkDataType>::FIrisNetworkComponentBuilder()
{
	static_assert(std::is_convertible_v<NetworkDataType*, FBasePIEIrisNetworkComponentState*>, "NetworkDataType must derive from FBaseNetworkComponentState");
}

template <typename NetworkDataType>
inline FIrisNetworkComponentBuilder<NetworkDataType>& FIrisNetworkComponentBuilder<NetworkDataType>::WithClients(int32 InClientCount)
{
	checkf(InClientCount > 0, TEXT("Client count must be greater than 0.  Server only tests should simply use a Spawner"));
	ClientCount = InClientCount;
	return *this;
}

template<typename NetworkDataType>
inline FIrisNetworkComponentBuilder<NetworkDataType>& FIrisNetworkComponentBuilder<NetworkDataType>::WithPacketSimulationSettings(FPacketSimulationSettings* InPacketSimulationSettings)
{
	PacketSimulationSettings = InPacketSimulationSettings;
	return *this;
}

template <typename NetworkDataType>
inline FIrisNetworkComponentBuilder<NetworkDataType>& FIrisNetworkComponentBuilder<NetworkDataType>::AsDedicatedServer()
{
	bIsDedicatedServer = true;
	return *this;
}

template <typename NetworkDataType>
inline FIrisNetworkComponentBuilder<NetworkDataType>& FIrisNetworkComponentBuilder<NetworkDataType>::AsListenServer()
{
	bIsDedicatedServer = false;
	return *this;
}

template <typename NetworkDataType>
inline FIrisNetworkComponentBuilder<NetworkDataType>& FIrisNetworkComponentBuilder<NetworkDataType>::WithGameMode(TSubclassOf<AGameModeBase> InGameMode)
{
	GameMode = InGameMode;
	return *this;
}

template <typename NetworkDataType>
inline FIrisNetworkComponentBuilder<NetworkDataType>& FIrisNetworkComponentBuilder<NetworkDataType>::WithGameInstanceClass(FSoftClassPath InGameInstanceClass)
{
	GameInstanceClass = InGameInstanceClass;
	return *this;
}

template<typename NetworkDataType>
inline void FIrisNetworkComponentBuilder<NetworkDataType>::Build(FPIEIrisNetworkComponent<NetworkDataType>& OutNetwork)
{
	NetworkDataType DefaultState{};
	DefaultState.ClientCount = ClientCount;
	DefaultState.bIsDedicatedServer = bIsDedicatedServer;

	OutNetwork.ServerState = MakeUnique<NetworkDataType>(DefaultState);
	OutNetwork.ServerState->ClientConnections.SetNum(ClientCount);

	for (int32 ClientIndex = 0; ClientIndex < ClientCount; ClientIndex++)
	{
		OutNetwork.ClientStates.Add(MakeUnique<NetworkDataType>(DefaultState));
		OutNetwork.ClientStates.Last()->ClientIndex = ClientIndex;
	}

	OutNetwork.PacketSimulationSettings = PacketSimulationSettings;
	OutNetwork.GameMode = GameMode;
	OutNetwork.StateRestorer = FPIENetworkTestStateRestorer{GameInstanceClass, GameMode};
}
