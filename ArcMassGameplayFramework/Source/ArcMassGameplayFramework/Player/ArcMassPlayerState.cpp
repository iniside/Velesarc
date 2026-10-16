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

#include "Player/ArcMassPlayerState.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"

#include "Experience/ArcMassExperienceDefinition.h"
#include "Experience/ArcMassExperienceManagerComponent.h"
#include "Pawn/ArcMassPawnData.h"
#include "Pawn/ArcMassPawnExtensionComponent.h"
#include "Player/ArcMassPlayerStateExtensionComponent.h"

DEFINE_LOG_CATEGORY(LogArcMassPlayerState);

AArcMassPlayerState::AArcMassPlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, MyPlayerConnectionType(EArcMassPlayerConnectionType::Player)
{
	SetNetUpdateFrequency(30.0f);
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	MyTeamId = INDEX_NONE;
	MySquadId = INDEX_NONE;

	MyPlayerStateExtensionComponent = CreateDefaultSubobject<UArcMassPlayerStateExtensionComponent>(TEXT("PlayerStateExtension"));
	MyPlayerStateExtensionComponent->SetIsReplicated(true);
}

void AArcMassPlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Authority-only: subscribe to the experience-loaded broadcast so we can mirror
	// the experience's DefaultPawnData into PawnData. Clients see the result via
	// OnRep_PawnData. AGameMode's GetPawnDataForController already runs the same
	// fallback at pawn-spawn time; populating PawnData here makes
	// MassPS->GetPawnData() queryable from any system that wants it.
	if (GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	UWorld* World = GetWorld();
	AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (GS == nullptr)
	{
		return;
	}

	if (UArcMassExperienceManagerComponent* ExpMgr = GS->FindComponentByClass<UArcMassExperienceManagerComponent>())
	{
		ExpMgr->CallOrRegister_OnExperienceLoaded(
			FOnArcMassExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::OnExperienceLoaded));
	}
}

void AArcMassPlayerState::BeginPlay()
{
	Super::BeginPlay();

	UE_LOGF(LogArcMassPlayerState, Log,"AArcMassPlayerState::BeginPlay CheckDataReady [%ls]", *GetNameSafe(this));
	CheckDataReady();
}

void AArcMassPlayerState::ClientInitialize(AController* C)
{
	Super::ClientInitialize(C);

	if (UArcMassPawnExtensionComponent* PawnExtComp = UArcMassPawnExtensionComponent::FindPawnExtensionComponent(GetPawn()))
	{
		PawnExtComp->CheckDefaultInitialization();
	}
}

void AArcMassPlayerState::CopyProperties(APlayerState* PlayerState)
{
	// Seamless travel intentionally not propagated yet — mirrors AArcCorePlayerState.
}

void AArcMassPlayerState::OnDeactivated()
{
	bool bDestroyDeactivatedPlayerState = false;

	switch (GetPlayerConnectionType())
	{
		case EArcMassPlayerConnectionType::Player:
		case EArcMassPlayerConnectionType::InactivePlayer:
			bDestroyDeactivatedPlayerState = false;
			break;
		default:
			bDestroyDeactivatedPlayerState = true;
			break;
	}

	SetPlayerConnectionType(EArcMassPlayerConnectionType::InactivePlayer);

	if (bDestroyDeactivatedPlayerState)
	{
		Destroy();
	}
}

void AArcMassPlayerState::OnReactivated()
{
	if (GetPlayerConnectionType() == EArcMassPlayerConnectionType::InactivePlayer)
	{
		SetPlayerConnectionType(EArcMassPlayerConnectionType::Player);
	}
}

void AArcMassPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;
	SharedParams.RepNotifyCondition = ELifetimeRepNotifyCondition::REPNOTIFY_Always;

	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, PawnData, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, MyPlayerConnectionType, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, MyTeamId, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, MySquadId, SharedParams);
}

void AArcMassPlayerState::SetPawnData(const UArcMassPawnData* InPawnData)
{
	check(InPawnData);

	// Authority-only. A client call would locally desync (no replication, no
	// CheckDataReady) and silently drift from the server's view of PawnData.
	if (GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	if (PawnData)
	{
		UE_LOGF(LogArcMassPlayerState
			, Log
			,"Trying to set PawnData [%ls] on player state [%ls] that already has valid PawnData [%ls].", *GetNameSafe(InPawnData)
			, *GetNameSafe(this)
			, *GetNameSafe(PawnData));
		return;
	}

	PawnData = InPawnData;
	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, PawnData, this);

	UE_LOGF(LogArcMassPlayerState, Log,"AArcMassPlayerState::SetPawnData CheckDataReady [%ls]", *GetNameSafe(this));
	CheckDataReady(InPawnData);

	ForceNetUpdate();
}

void AArcMassPlayerState::OnRep_PawnData()
{
	UE_LOGF(LogArcMassPlayerState, Log,"AArcMassPlayerState::OnRep_PawnData CheckDataReady [%ls]", *GetNameSafe(this));
	CheckDataReady(PawnData);
}

void AArcMassPlayerState::CheckDataReady(const UArcMassPawnData* InPawnData)
{
	if (PawnData == nullptr && InPawnData != nullptr)
	{
		PawnData = InPawnData;
	}

	// PawnData may have just become valid — nudge the PS-side init state chain so the
	// PS extension's Spawned->DataAvailable gate can fire (and its siblings can follow).
	// CheckDefaultInitialization is idempotent.
	if (MyPlayerStateExtensionComponent != nullptr)
	{
		MyPlayerStateExtensionComponent->CheckDefaultInitialization();
	}

	UE_LOGF(LogArcMassPlayerState
		, Log
		,"AArcMassPlayerState::CheckDataReady [Pawn %ls] -- [%ls] -- [PawnData %ls]", *GetNameSafe(GetPawn())
		, *GetNameSafe(this)
		, *GetNameSafe(PawnData));

	if (PawnData != nullptr && bPawnDataInitialized == false && GetPawn() != nullptr)
	{
		bPawnDataInitialized = true;

		if (UArcMassPawnExtensionComponent* PawnExtension = UArcMassPawnExtensionComponent::FindPawnExtensionComponent(GetPawn()))
		{
			// PawnExtension hooks into Pawn data ready via SetPawnData on authority side.
			PawnExtension->CheckDefaultInitialization();
		}
	}
}

void AArcMassPlayerState::SetPlayerConnectionType(EArcMassPlayerConnectionType NewType)
{
	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, MyPlayerConnectionType, this);
	MyPlayerConnectionType = NewType;
}

void AArcMassPlayerState::SetSquadId(int32 NewSquadId)
{
	if (HasAuthority())
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, MySquadId, this);
		MySquadId = NewSquadId;
	}
}

void AArcMassPlayerState::SetTeamId(int32 NewTeamId)
{
	if (HasAuthority())
	{
		const int32 OldTeamId = MyTeamId;
		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, MyTeamId, this);

		MyTeamId = NewTeamId;
		UE_LOGF(LogArcMassPlayerState, Log,"[Server] ArcMassPS %ls assigned team %d", *GetUniqueId().ToDebugString(), MyTeamId);

		OnTeamChangedDelegate.Broadcast(OldTeamId, MyTeamId);
	}
}

void AArcMassPlayerState::OnRep_TeamId(int32 OldTeamId)
{
	UE_LOGF(LogArcMassPlayerState
		, Log
		,"[Client] ArcMassPS %ls assigned team %d", *GetUniqueId().ToDebugString()
		, MyTeamId);

	OnTeamChangedDelegate.Broadcast(OldTeamId, MyTeamId);
}

void AArcMassPlayerState::OnRep_SquadId()
{
	// TODO: notify squad subsystem if/when one exists.
}

void AArcMassPlayerState::OnExperienceLoaded(const UArcMassExperienceDefinition* InExperience)
{
	// Authority-only; SetPawnData itself enforces this, but bail early to avoid
	// pointless work on clients that won't get this delegate anyway (we only
	// register on authority).
	if (InExperience == nullptr || GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	// Don't clobber a PawnData another system already chose for this player.
	if (PawnData != nullptr)
	{
		return;
	}

	const TSoftObjectPtr<UArcMassPawnData>& DefaultRef = InExperience->GetDefaultPawnData();
	if (DefaultRef.IsNull())
	{
		return;
	}

	// The experience manager already loads DefaultPawnData before broadcasting
	// OnExperienceLoaded (OnExperienceLoadComplete waits on it). Get() should
	// resolve; LoadSynchronous is a defensive fallback.
	UArcMassPawnData* Resolved = DefaultRef.Get();
	if (Resolved == nullptr)
	{
		Resolved = DefaultRef.LoadSynchronous();
	}

	if (Resolved != nullptr)
	{
		SetPawnData(Resolved);
	}
}
