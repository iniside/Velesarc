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


#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "ModularPlayerState.h"
#include "Components/PlayerStateComponent.h"
#include "ArcCorePlayerState.generated.h"

class AArcCorePlayerController;
class UArcCoreAbilitySystemComponent;
class UAbilitySystemComponent;
class UArcPawnData;
class UArcExperienceDefinition;

DECLARE_LOG_CATEGORY_EXTERN(LogArcPlayerState
	, Log
	, Log);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTeamChanged
	, int32
	, OldTeam
	, int32
	, NewTeam);

/** Defines the types of client connected */
UENUM()
enum class EArcPlayerConnectionType : uint8
{
	// An active player
	Player = 0
	// Spectator connected to a running game
	, LiveSpectator
	
	// Spectating a demo recording offline
	, ReplaySpectator
	
	// A deactivated player (disconnected)
	, InactivePlayer
};

/**
 *
 */
UCLASS()
class ARCCORE_API AArcCorePlayerState
	: public AModularPlayerState
	, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

public:
	AArcCorePlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	virtual void PreRegisterAllComponents() override;
	virtual void OnSetUniqueId() override;
	virtual void OnRep_UniqueId() override;
	UFUNCTION(Server, Reliable)
	void ServerSendTokenAuth(const FName& Type, const FString& Token);
	
	AArcCorePlayerController* GetArcPlayerController() const;

	/*
	 * Try to get Ability System Component. If not set will try to find component first
	 * and then assign it to variable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Player State")
	UArcCoreAbilitySystemComponent* GetArcAbilitySystemComponent() const;

	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
	
	template <class T = UArcPawnData>
	const T* GetPawnData() const
	{
		return Cast<T>(PawnData);
	}

	void SetPawnData(const UArcPawnData* InPawnData);

	//~AActor interface
	virtual void PreInitializeComponents() override;

	virtual void PostInitializeComponents() override;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of AActor interface

	UFUNCTION(Server, Reliable)
	void ServerSetUniqueNetId(const FUniqueNetIdRepl& NetId);
	
	virtual void PostNetInit() override;
	virtual void BeginReplication() override;
	
	//~APlayerState interface
	virtual void Reset() override;

	virtual void ClientInitialize(AController* C) override;

	virtual void CopyProperties(APlayerState* PlayerState) override;

	virtual void OnDeactivated() override;

	virtual void OnReactivated() override;

	//~End of APlayerState interface

	static const FName NAME_ArcAbilityReady;

	void SetPlayerConnectionType(EArcPlayerConnectionType NewType);

	EArcPlayerConnectionType GetPlayerConnectionType() const
	{
		return MyPlayerConnectionType;
	}

	/** Returns the Squad ID of the squad the player belongs to. */
	UFUNCTION(BlueprintCallable)
	int32 GetSquadId() const
	{
		return MySquadId;
	}

	/** Returns the Squad ID of the squad the player belongs to. */
	UFUNCTION(BlueprintCallable)
	int32 GetTeamId() const
	{
		return MyTeamId;
	}

	void SetSquadId(int32 NewSquadId);

	void SetTeamId(int32 NewTeamId);

	FOnTeamChanged& OnTeamChanged()
	{
		return OnTeamChangedDelegate;
	}

	// Send a message to just this player
	// (use only for client notifications like accolades, quest toasts, etc... that can
	// handle being occasionally lost) UFUNCTION(Client, Unreliable, BlueprintCallable,
	// Category = "Arc Core|PlayerState") void ClientBroadcastMessage(const FArcVerbMessage
	// Message);

private:
	void OnExperienceLoaded(const UArcExperienceDefinition* CurrentExperience);

public:
	/**
	 * Called from PlayerState PlayerController and Character to see if we have all data ready on clients.
	 * Function is protecting itself against reentry, so it is safe to call multiple times over.
	 */
	void CheckDataReady(const UArcPawnData* InPawnData = nullptr);
	
protected:
	UFUNCTION()
	virtual void OnRep_PawnData();
			
protected:
	UPROPERTY(ReplicatedUsing = OnRep_PawnData)
	TObjectPtr<const UArcPawnData> PawnData;
	
private:
	UPROPERTY(VisibleAnywhere, Category = "PlayerState")
	mutable TObjectPtr<UArcCoreAbilitySystemComponent> AbilitySystemComponent;

	bool bDataIsReady = false;

	UPROPERTY()
	bool bPawnDataInitialized = false;
	
	UPROPERTY(Replicated)
	EArcPlayerConnectionType MyPlayerConnectionType;

	UPROPERTY(ReplicatedUsing = OnRep_TeamId)
	int32 MyTeamId;

	UPROPERTY(ReplicatedUsing = OnRep_SquadId)
	int32 MySquadId;

	UFUNCTION()
	void OnRep_TeamId(int32 OldTeamId);

	UFUNCTION()
	void OnRep_SquadId();

	UPROPERTY()
	FOnTeamChanged OnTeamChangedDelegate;

public:
	bool GetPawnDataInitialized() const
	{
		return bPawnDataInitialized;
	}
};

UCLASS(ClassGroup=(ArcCore), meta=(BlueprintSpawnableComponent))
class ARCCORE_API UArcCorePlayerStateComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

public:
	virtual void OnPawnDataReady(APawn* InPawn) {}
};
