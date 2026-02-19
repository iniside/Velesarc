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

#include "ArcPawnExtensionComponent.h"

#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "ArcCoreGameplayTags.h"
#include "ArcEventTags.h"
#include "ArcLogs.h"
#include "ArcPawnData.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameMode/ArcExperienceManagerComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Player/ArcCorePlayerState.h"

DEFINE_LOG_CATEGORY(LogPawnExtenstionComponent)

const FName UArcPawnExtensionComponent::NAME_ActorFeatureName("PawnExtension");

UArcPawnExtensionComponent::UArcPawnExtensionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);

	PawnData = nullptr;
	AbilitySystemComponent = nullptr;
	bPawnReadyToInitialize = false;
}

void UArcPawnExtensionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;
	SharedParams.RepNotifyCondition = REPNOTIFY_Always;
	
	DOREPLIFETIME_WITH_PARAMS_FAST(UArcPawnExtensionComponent
		, PawnData
		, SharedParams);
}

void UArcPawnExtensionComponent::OnRegister()
{
	Super::OnRegister();

	const APawn* Pawn = GetPawn<APawn>();
	ensureAlwaysMsgf((Pawn != nullptr)
		, TEXT("ArcPawnExtensionComponent on [%s] can only be added to Pawn actors.")
		, *GetNameSafe(GetOwner()));

	TArray<UActorComponent*> PawnExtensionComponents;
	Pawn->GetComponents(UArcPawnExtensionComponent::StaticClass()
		, PawnExtensionComponents);
	ensureAlwaysMsgf((PawnExtensionComponents.Num() == 1)
		, TEXT("Only one ArcPawnExtensionComponent should exist on [%s].")
		, *GetNameSafe(GetOwner()));

	// Register with the init state system early, this will only work if this is a game
	// world
	RegisterInitStateFeature();
}

void UArcPawnExtensionComponent::BeginPlay()
{
	Super::BeginPlay();

	// Listen for changes to all features
	BindOnActorInitStateChanged(NAME_None
		, FGameplayTag()
		, false);

	// Notifies state manager that we have spawned, then try rest of default
	// initialization
	ensure(TryToChangeInitState(FArcCoreGameplayTags::Get().InitState_Spawned));
	CheckDefaultInitialization();
}

void UArcPawnExtensionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UninitializeAbilitySystem();
	UnregisterInitStateFeature();

	Super::EndPlay(EndPlayReason);
}

void UArcPawnExtensionComponent::SetPawnData(const UArcPawnData* InPawnData)
{
	check(InPawnData);

	APawn* Pawn = GetPawnChecked<APawn>();

	if (Pawn->GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	if (PawnData)
	{
		UE_LOG(LogPawnExtenstionComponent
			, Error
			, TEXT( "Trying to set PawnData [%s] on pawn [%s] that already has valid PawnData [%s].")
			, *GetNameSafe(InPawnData)
			, *GetNameSafe(Pawn)
			, *GetNameSafe(PawnData));
		return;
	}

	PawnData = InPawnData;
	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass
			, PawnData
			, this);
	
	Pawn->ForceNetUpdate();

	CheckDefaultInitialization();

	LOG_ARC_NET(LogPawnExtenstionComponent
		, "UArcPawnExtensionComponent::SetPawnData")
}

void UArcPawnExtensionComponent::OnRep_PawnData()
{
	CheckDefaultInitialization();
	if(AArcCorePlayerState* ArcPS = GetPlayerState<AArcCorePlayerState>())
	{
		UE_LOG(LogPawnExtenstionComponent, Log, TEXT("UArcPawnExtensionComponent::OnRep_PawnData CheckDataReady"))
		ArcPS->CheckDataReady();
	}
}

void UArcPawnExtensionComponent::InitializeAbilitySystem(UArcCoreAbilitySystemComponent* InASC
														 , AActor* InOwnerActor)
{
	check(InASC);
	check(InOwnerActor);

	if (AbilitySystemComponent == InASC)
	{
		// The ability system component hasn't changed.
		return;
	}

	if (AbilitySystemComponent)
	{
		// Clean up the old ability system component.
		UninitializeAbilitySystem();
	}

	APawn* Pawn = GetPawnChecked<APawn>();
	AActor* ExistingAvatar = InASC->GetAvatarActor();

	UE_LOG(LogPawnExtenstionComponent
		, Log
		, TEXT("Setting up ASC [%s] on pawn [%s] owner [%s], existing [%s] ")
		, *GetNameSafe(InASC)
		, *GetNameSafe(Pawn)
		, *GetNameSafe(InOwnerActor)
		, *GetNameSafe(ExistingAvatar));

	if (ExistingAvatar != nullptr && ExistingAvatar != Pawn)
	{
		UE_LOG(LogPawnExtenstionComponent
			, Log
			, TEXT("Existing avatar (authority=%d)")
			, ExistingAvatar->HasAuthority() ? 1 : 0);
		// This can happen on clients if they're lagged
		ensure(!ExistingAvatar->HasAuthority());

		UArcPawnExtensionComponent* OtherExtensionComponent = FindPawnExtensionComponent(ExistingAvatar);
		if (OtherExtensionComponent)
		{
			OtherExtensionComponent->UninitializeAbilitySystem();
		}
	}

	AbilitySystemComponent = InASC;
	AbilitySystemComponent->InitAbilityActorInfo(InOwnerActor
		, Pawn);

	// if (ensure(PawnData))
	//{
	//	InASC->SetTagRelationshipMapping(PawnData->TagRelationshipMapping);
	//}

	OnAbilitySystemInitialized.Broadcast();
}

void UArcPawnExtensionComponent::UninitializeAbilitySystem()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	UE_LOG(LogPawnExtenstionComponent
		, Log
		, TEXT("UArcPawnExtensionComponent::UninitializeAbilitySystem Avatar %s -- Owner %s")
		, *GetNameSafe(AbilitySystemComponent->GetAvatarActor())
		, *GetNameSafe(GetPawnChecked<APawn>()));
	
	if (AbilitySystemComponent->GetAvatarActor() == GetOwner())
	{
		FGameplayTagContainer NoTags(Arcx::Tags::GameplayEffect_Death_Persistent);
		FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchNoEffectTags(NoTags);

		AbilitySystemComponent->RemoveActiveEffects(Query);

		AbilitySystemComponent->CancelAbilities(nullptr, nullptr);
		AbilitySystemComponent->ClearAbilityInput();
		AbilitySystemComponent->RemoveAllGameplayCues();

		//if (AbilitySystemComponent->GetOwnerActor() != nullptr)
		//{
		//	AbilitySystemComponent->SetAvatarActor(nullptr);
		//}
		//else
		//{
		//	// If the ASC doesn't have a valid owner, we need to clear *all* actor info, not
		//	// just the avatar pairing
		//	AbilitySystemComponent->ClearActorInfo();
		//}

		OnAbilitySystemUninitialized.Broadcast();
	}
	//AbilitySystemComponent = nullptr;
}

void UArcPawnExtensionComponent::HandleControllerChanged()
{
	if (AbilitySystemComponent && (AbilitySystemComponent->GetAvatarActor() == GetPawnChecked<APawn>()))
	{
		ensure(AbilitySystemComponent->AbilityActorInfo->OwnerActor == AbilitySystemComponent->GetOwnerActor());
		if (AbilitySystemComponent->GetOwnerActor() == nullptr)
		{
			UninitializeAbilitySystem();
		}
		else
		{
			AbilitySystemComponent->RefreshAbilityActorInfo();
		}
	}

	LOG_ARC_NET(LogPawnExtenstionComponent
		, "UArcPawnExtensionComponent::HandleControllerChanged")
	CheckDefaultInitialization();
}

void UArcPawnExtensionComponent::HandlePlayerStateReplicated()
{
	LOG_ARC_NET(LogPawnExtenstionComponent
		, "UArcPawnExtensionComponent::HandlePlayerStateReplicated")
	CheckDefaultInitialization();
}

void UArcPawnExtensionComponent::SetupPlayerInputComponent()
{
	LOG_ARC_NET(LogPawnExtenstionComponent
		, "UArcPawnExtensionComponent::SetupPlayerInputComponent")
	CheckDefaultInitialization();
}

void UArcPawnExtensionComponent::HandlePlayerStateReplicatedFromController()
{
	LOG_ARC_NET(LogPawnExtenstionComponent
		, "UArcPawnExtensionComponent::SetupPlayerInputComponent")
	CheckDefaultInitialization();
}

void UArcPawnExtensionComponent::HandlePawnReplicatedFromController()
{
	LOG_ARC_NET(LogPawnExtenstionComponent
		, "UArcPawnExtensionComponent::HandlePawnReplicatedFromController")
	CheckDefaultInitialization();
}

void UArcPawnExtensionComponent::HandlePawnDataReplicated()
{
	LOG_ARC_NET(LogPawnExtenstionComponent
	, "UArcPawnExtensionComponent::HandlePawnDataReplicated")
	CheckDefaultInitialization();
}

void UArcPawnExtensionComponent::HandleMassEntityCreated()
{
	CheckDefaultInitialization();
}

// TODO might want to re added to new init system.
void UArcPawnExtensionComponent::WaitForExperienceLoad()
{
	AGameStateBase* GS = GetWorld()->GetGameState();
	if (GS == nullptr)
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(this
			, &UArcPawnExtensionComponent::WaitForExperienceLoad);
		return;
	}

	UArcExperienceManagerComponent* ArcEMC = GS->FindComponentByClass<UArcExperienceManagerComponent>();

	if (ArcEMC->IsExperienceLoaded() == false)
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(this
			, &UArcPawnExtensionComponent::WaitForExperienceLoad);
		return;
	}
	CheckDefaultInitialization();
}

void UArcPawnExtensionComponent::OnPawnReadyToInitialize_RegisterAndCall(FSimpleMulticastDelegate::FDelegate Delegate)
{
	if (!OnPawnReadyToInitialize.IsBoundToObject(Delegate.GetUObject()))
	{
		OnPawnReadyToInitialize.Add(Delegate);
	}

	if (bPawnReadyToInitialize)
	{
		Delegate.Execute();
	}
}

void UArcPawnExtensionComponent::OnAbilitySystemInitialized_RegisterAndCall(
	FSimpleMulticastDelegate::FDelegate Delegate)
{
	if (!OnAbilitySystemInitialized.IsBoundToObject(Delegate.GetUObject()))
	{
		OnAbilitySystemInitialized.Add(Delegate);
	}

	if (AbilitySystemComponent)
	{
		Delegate.Execute();
	}
}

void UArcPawnExtensionComponent::OnAbilitySystemUninitialized_Register(FSimpleMulticastDelegate::FDelegate Delegate)
{
	if (!OnAbilitySystemUninitialized.IsBoundToObject(Delegate.GetUObject()))
	{
		OnAbilitySystemUninitialized.Add(Delegate);
	}
}

void UArcPawnExtensionComponent::CheckDefaultInitialization()
{
	// Before checking our progress, try progressing any other features we might depend on
	CheckDefaultInitializationForImplementers();

	const FArcCoreGameplayTags& InitTags = FArcCoreGameplayTags::Get();
	static const TArray<FGameplayTag> StateChain = {
		InitTags.InitState_Spawned
		, InitTags.InitState_DataAvailable
		, InitTags.InitState_DataInitialized
		, InitTags.InitState_GameplayReady
	};

	// This will try to progress from spawned (which is only set in BeginPlay) through the
	// data initialization stages until it gets to gameplay ready
	ContinueInitStateChain(StateChain);
}

bool UArcPawnExtensionComponent::CanChangeInitState(UGameFrameworkComponentManager* Manager
													, FGameplayTag CurrentState
													, FGameplayTag DesiredState) const
{
	check(Manager);

	APawn* Pawn = GetPawn<APawn>();
	const FArcCoreGameplayTags& InitTags = FArcCoreGameplayTags::Get();

	if (!CurrentState.IsValid() && DesiredState == InitTags.InitState_Spawned)
	{
		// As long as we are on a valid pawn, we count as spawned
		if (Pawn)
		{
			LOG_ARC_NET(LogPawnExtenstionComponent
				, "UArcPawnExtensionComponent::CanChangeInitState [CurrentState %s] [DesiredState %s] [Pawn %s]"
				, *CurrentState.ToString()
				, *DesiredState.ToString()
				, *GetNameSafe(Pawn))
			return true;
		}
	}
	if (CurrentState == InitTags.InitState_Spawned && DesiredState == InitTags.InitState_DataAvailable)
	{
		// Pawn data is required.
		if (!PawnData)
		{
			LOG_ARC_NET(LogPawnExtenstionComponent
				, "UArcPawnExtensionComponent::CanChangeInitState [CurrentState %s] [DesiredState %s] invalid PawnData"
				, *CurrentState.ToString()
				, *DesiredState.ToString())
			return false;
		}

		const bool bHasAuthority = Pawn->HasAuthority();
		const bool bIsLocallyControlled = Pawn->IsLocallyControlled();

		if (bHasAuthority || bIsLocallyControlled)
		{
			// Check for being possessed by a controller.
			if (!GetController<AController>())
			{
				LOG_ARC_NET(LogPawnExtenstionComponent
					, "UArcPawnExtensionComponent::CanChangeInitState [CurrentState %s] [DesiredState %s] not possessed "
					"by controller"
					, *CurrentState.ToString()
					, *DesiredState.ToString())
				return false;
			}
		}

		return true;
	}
	else if (CurrentState == InitTags.InitState_DataAvailable && DesiredState == InitTags.InitState_DataInitialized)
	{
		// Transition to initialize if all features have their data available

		bool bInited = Manager->HaveAllFeaturesReachedInitState(Pawn
			, InitTags.InitState_DataAvailable);
		//LOG_ARC_NET(LogPawnExtenstionComponent
		//	, "UArcPawnExtensionComponent::CanChangeInitState [CurrentState %s] [DesiredState %s] All features "
		//	"Initialized [%s]"
		//	, *CurrentState.ToString()
		//	, *DesiredState.ToString()
		//	, bInited ? "TRUE" : "FALSE")

		return bInited;
	}
	else if (CurrentState == InitTags.InitState_DataInitialized && DesiredState == InitTags.InitState_GameplayReady)
	{
		LOG_ARC_NET(LogPawnExtenstionComponent
			, "UArcPawnExtensionComponent::CanChangeInitState [CurrentState %s] [DesiredState %s] success"
			, *CurrentState.ToString()
			, *DesiredState.ToString())
		return true;
	}

	LOG_ARC_NET(LogPawnExtenstionComponent
		, "UArcPawnExtensionComponent::CanChangeInitState [CurrentState %s] [DesiredState %s] failed"
		, *CurrentState.ToString()
		, *DesiredState.ToString())
	return false;
}

void UArcPawnExtensionComponent::HandleChangeInitState(UGameFrameworkComponentManager* Manager
													   , FGameplayTag CurrentState
													   , FGameplayTag DesiredState)
{
	if (DesiredState == FArcCoreGameplayTags::Get().InitState_DataInitialized)
	{
		bPawnReadyToInitialize = true;
		// This is currently all handled by other components listening to this state
		// change
	}
}

void UArcPawnExtensionComponent::OnActorInitStateChanged(const FActorInitStateChangedParams& Params)
{
	// If another feature is now in DataAvailable, see if we should transition to
	// DataInitialized
	if (Params.FeatureName != NAME_ActorFeatureName)
	{
		const FArcCoreGameplayTags& InitTags = FArcCoreGameplayTags::Get();
		if (Params.FeatureState == InitTags.InitState_DataAvailable)
		{
			CheckDefaultInitialization();
		}
	}
}
