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

#include "ArcHealthComponent.h"

#include "ArcEventTags.h"
#include "AsyncMessageSystemBase.h"
#include "AsyncMessageWorldSubsystem.h"
#include "GameplayTagsManager.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameplayMessageSubsystem.h"

void UArcAttributeHandlingComponent::OnRegister()
{
	Super::OnRegister();
	IsPawnComponentReadyToInitialize();
}

void UArcAttributeHandlingComponent::BeginPlay()
{
	Super::BeginPlay();
	IsPawnComponentReadyToInitialize();

	
}

bool UArcAttributeHandlingComponent::IsPawnComponentReadyToInitialize()
{
	// GAS retired (Bucket D 7); attribute handling pending Mass-side reimplementation.
	return false;
}

void UArcAttributeHandlingComponent::OnPawnReady()
{
	// GAS retired (Bucket D 7); attribute handling pending Mass-side reimplementation.
}

DEFINE_LOG_CATEGORY(LogArcDeath);

void UArcHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UArcHealthComponent, DeathState);
}

void UArcHealthComponent::OnRep_DeathState(EArcDeathState OldDeathState)
{
	const EArcDeathState NewDeathState = DeathState;

	// Revert the death state for now since we rely on StartDeath and FinishDeath to
	// change it.
	DeathState = OldDeathState;

	if (OldDeathState > NewDeathState)
	{
		// The server is trying to set us back but we've already predicted past the server
		// state.
		UE_LOGF(LogArcDeath
			, Warning
			, "OnRep_DeathState: Predicted past server death state [%d] -> [%d] for owner [%ls].", (uint8)OldDeathState
			, (uint8)NewDeathState
			, *GetNameSafe(GetOwner()));
		return;
	}

	if (OldDeathState == EArcDeathState::NotDead)
	{
		if (NewDeathState == EArcDeathState::DeathStarted)
		{
			StartDeath();
		}
		else if (NewDeathState == EArcDeathState::DeathFinished)
		{
			StartDeath();
			FinishDeath();
		}
		else
		{
			UE_LOGF(LogArcDeath
				, Error
				, "OnRep_DeathState: Invalid death transition [%d] -> [%d] for owner [%ls].", (uint8)OldDeathState
				, (uint8)NewDeathState
				, *GetNameSafe(GetOwner()));
		}
	}
	else if (OldDeathState == EArcDeathState::DeathStarted)
	{
		if (NewDeathState == EArcDeathState::DeathFinished)
		{
			FinishDeath();
		}
		else
		{
			UE_LOGF(LogArcDeath
				, Error
				, "OnRep_DeathState: Invalid death transition [%d] -> [%d] for owner [%ls].", (uint8)OldDeathState
				, (uint8)NewDeathState
				, *GetNameSafe(GetOwner()));
		}
	}

	ensureMsgf((DeathState == NewDeathState)
		, TEXT("OnRep_DeathState: Death transition failed [%d] -> [%d] for owner [%s].")
		, (uint8)OldDeathState
		, (uint8)NewDeathState
		, *GetNameSafe(GetOwner()));
}

void UArcHealthComponent::SetDeathState(EArcDeathState InNewState)
{
	DeathState = InNewState;
}

UArcHealthComponent::UArcHealthComponent()
{
}

void UArcHealthComponent::StartDeath()
{
	if (DeathState != EArcDeathState::NotDead)
	{
		return;
	}

	DeathState = EArcDeathState::DeathStarted;
	APawn* P = Cast<APawn>(GetOwner());
	if (P)
	{
		CachedController = P->GetController();
		OnDeathStart(P
			, P->GetController());
	}
	OnDeathStarted.Broadcast(GetOwner());

	// GAS retired; ASC uninitialize on death no longer needed.

	FTimerHandle Unused;
	GetWorld()->GetTimerManager().SetTimer(Unused
		, FTimerDelegate::CreateUObject(this
			, &UArcHealthComponent::FinishDeath)
		, TimeToDie
		, false);

	GetOwner()->ForceNetUpdate();
}

void UArcHealthComponent::FinishDeath()
{
	if (DeathState != EArcDeathState::DeathStarted)
	{
		return;
	}

	DeathState = EArcDeathState::DeathFinished;

	APawn* P = Cast<APawn>(GetOwner());
	if (P)
	{
		OnDeathFinished.Broadcast(GetOwner());
		OnDeathFinish(P
			, CachedController.Get());
	}
	if (HasAuthority())
	{
		// GAS retired; ASC uninitialize on death no longer needed.
		if (bDestroyOnDeath)
		{
			GetOwner()->Destroy(true);
		}
		else
		{
			//GetOwner()->SetActorHiddenInGame(true);
			//GetOwner()->SetActorEnableCollision(false);
		}
	}
	GetOwner()->ForceNetUpdate();
}

void UArcHealthComponent::HandleOwnerEndPlay(AActor* Actor
											 , EEndPlayReason::Type EndPlayReason)
{
	FinishDeath();
}
