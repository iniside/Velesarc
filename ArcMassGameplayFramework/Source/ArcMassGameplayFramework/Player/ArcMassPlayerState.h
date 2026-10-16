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

#include "GameplayTagContainer.h"
#include "ModularPlayerState.h"

#include "ArcMassPlayerState.generated.h"

class APawn;
class UArcMassPawnData;
class UArcMassPlayerStateExtensionComponent;
class UArcMassExperienceDefinition;

DECLARE_LOG_CATEGORY_EXTERN(LogArcMassPlayerState, Log, Log);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FArcMassOnTeamChanged
	, int32, OldTeam
	, int32, NewTeam);

/** Defines the types of client connected. Mirrors AArcCorePlayerState's enum without GAS coupling. */
UENUM()
enum class EArcMassPlayerConnectionType : uint8
{
	// An active player
	Player = 0,
	// Spectator connected to a running game
	LiveSpectator,
	// Spectating a demo recording offline
	ReplaySpectator,
	// A deactivated player (disconnected)
	InactivePlayer
};

/**
 * AArcMassPlayerState
 *
 * Modular PlayerState for the Mass-driven game framework. Holds the replicated
 * UArcMassPawnData reference and team/squad/connection-type state. Intentionally
 * has NO AbilitySystemComponent and does NOT implement IAbilitySystemInterface —
 * gameplay state lives on Mass entities.
 */
UCLASS()
class ARCMASSGAMEPLAYFRAMEWORK_API AArcMassPlayerState : public AModularPlayerState
{
	GENERATED_BODY()

public:
	AArcMassPlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~AActor interface
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	//~End of AActor interface

	//~APlayerState interface
	virtual void ClientInitialize(AController* C) override;
	virtual void CopyProperties(APlayerState* PlayerState) override;
	virtual void OnDeactivated() override;
	virtual void OnReactivated() override;
	//~End of APlayerState interface

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	template <class T = UArcMassPawnData>
	const T* GetPawnData() const
	{
		return Cast<T>(PawnData);
	}

	/** Server-only assignment of PawnData. Triggers replication and CheckDataReady on clients via OnRep. */
	void SetPawnData(const UArcMassPawnData* InPawnData);

	/**
	 * Called from PlayerState/PlayerController/Pawn paths to verify data is ready on clients.
	 * Re-entrant safe via bPawnDataInitialized.
	 */
	void CheckDataReady(const UArcMassPawnData* InPawnData = nullptr);

	void SetPlayerConnectionType(EArcMassPlayerConnectionType NewType);

	EArcMassPlayerConnectionType GetPlayerConnectionType() const
	{
		return MyPlayerConnectionType;
	}

	UFUNCTION(BlueprintCallable)
	int32 GetSquadId() const
	{
		return MySquadId;
	}

	UFUNCTION(BlueprintCallable)
	int32 GetTeamId() const
	{
		return MyTeamId;
	}

	void SetSquadId(int32 NewSquadId);
	void SetTeamId(int32 NewTeamId);

	FArcMassOnTeamChanged& OnTeamChanged()
	{
		return OnTeamChangedDelegate;
	}

	bool GetPawnDataInitialized() const
	{
		return bPawnDataInitialized;
	}

protected:
	UFUNCTION()
	virtual void OnRep_PawnData();

	UFUNCTION()
	void OnRep_TeamId(int32 OldTeamId);

	UFUNCTION()
	void OnRep_SquadId();

	/** Authority-only: mirror experience DefaultPawnData into PawnData unless already set. */
	void OnExperienceLoaded(const UArcMassExperienceDefinition* InExperience);

protected:
	UPROPERTY(ReplicatedUsing = OnRep_PawnData)
	TObjectPtr<const UArcMassPawnData> PawnData;

private:
	// Single-shot per PlayerState lifetime. CheckDataReady is re-entrant by design
	// (called from PlayerState/PlayerController/Pawn paths and from OnRep_PawnData),
	// and this latch ensures the initial-ready broadcast fires exactly once. Not
	// reset on Reset()/seamless travel — if either is added later, clear here too.
	UPROPERTY()
	bool bPawnDataInitialized = false;

	UPROPERTY(Replicated)
	EArcMassPlayerConnectionType MyPlayerConnectionType;

	UPROPERTY(ReplicatedUsing = OnRep_TeamId)
	int32 MyTeamId;

	UPROPERTY(ReplicatedUsing = OnRep_SquadId)
	int32 MySquadId;

	UPROPERTY()
	FArcMassOnTeamChanged OnTeamChangedDelegate;

	UPROPERTY(VisibleAnywhere, Category = "ArcMass|PlayerState")
	TObjectPtr<UArcMassPlayerStateExtensionComponent> MyPlayerStateExtensionComponent;
};
