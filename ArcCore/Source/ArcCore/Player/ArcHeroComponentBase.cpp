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

#include "ArcHeroComponentBase.h"
#include "AbilitySystemInterface.h"
#include "Components/GameFrameworkComponentManager.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/Pawn.h"
#include "Logging/MessageLog.h"

#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "AnimCharacterMovementLibrary.h"
#include "Player/ArcCorePlayerState.h"
#include "ArcCoreGameplayTags.h"
#include "ArcLogs.h"
#include "ArcCorePlayerController.h"
#include "InputMappingContext.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Player/ArcPlayerStateExtensionComponent.h"
#include "Input/ArcCoreInputComponent.h"
#include "Pawn/ArcPawnData.h"
#include "Pawn/ArcPawnExtensionComponent.h"
#include "GameFramework/PlayerController.h"
#include "KismetAnimationLibrary.h"

#include "PlayerMappableInputConfig.h"

#include "GameMode/ArcExperienceData.h"
#include "Misc/UObjectToken.h"
#include "Player/ArcLocalPlayerBase.h"
#include "Items/ArcItemDefinition.h"

#include "UI/ArcHUDBase.h"
#include "UserSettings/EnhancedInputUserSettings.h"

DEFINE_LOG_CATEGORY(LogArcHero);

namespace ArcxHero
{
	static const float LookYawRate = 300.0f;
	static const float LookPitchRate = 165.0f;
}; // namespace ArcxHero

const FName UArcHeroComponentBase::NAME_BindInputsNow("BindInputsNow");
const FName UArcHeroComponentBase::NAME_PlayerHeroCharacterInitialized("PlayerHeroCharacterInitialized");
const FName UArcHeroComponentBase::NAME_PlayerHeroPlayerStateInitialized("PlayerHeroPlayerStateInitialized");
const FName UArcHeroComponentBase::NAME_ActorFeatureName("Hero");

UArcHeroComponentBase::UArcHeroComponentBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bPawnHasInitialized = false;
	bReadyToBindInputs = false;
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

bool UArcHeroComponentBase::CanChangeInitState(UGameFrameworkComponentManager* Manager
											   , FGameplayTag CurrentState
											   , FGameplayTag DesiredState) const
{
	check(Manager);

	const FArcCoreGameplayTags& InitTags = FArcCoreGameplayTags::Get();
	APawn* Pawn = GetPawn<APawn>();

	if (!CurrentState.IsValid() && DesiredState == InitTags.InitState_Spawned)
	{
		// As long as we have a real pawn, let us transition
		if (Pawn)
		{
			LOG_ARC_NET(LogPawnExtenstionComponent
				, "UArcHeroComponentBase::CanChangeInitState [CurrentState %s] [DesiredState %s] Pawn [%s]"
				, *CurrentState.ToString()
				, *DesiredState.ToString()
				, *GetNameSafe(Pawn))
			return true;
		}
	}
	else if (CurrentState == InitTags.InitState_Spawned && DesiredState == InitTags.InitState_DataAvailable)
	{
		// The player state is required.
		AArcCorePlayerState* ArcPS = GetPlayerState<AArcCorePlayerState>();
		if (ArcPS == nullptr)
		{
			LOG_ARC_NET(LogPawnExtenstionComponent
				, "UArcHeroComponentBase::CanChangeInitState [CurrentState %s] [DesiredState %s] Invalid Player State"
				, *CurrentState.ToString()
				, *DesiredState.ToString())
			return false;
		}

		if(ArcPS->GetPawnDataInitialized() == false)
		{
			LOG_ARC_NET(LogPawnExtenstionComponent
				, "UArcHeroComponentBase::CanChangeInitState [CurrentState %s] [DesiredState %s] Data not yet confirmed."
				, *CurrentState.ToString()
				, *DesiredState.ToString())
			return false;
		}

		// If we're authority or autonomous, we need to wait for a controller with
		// registered ownership of the player state.
		if (Pawn->GetLocalRole() != ROLE_SimulatedProxy)
		{
			AController* Controller = GetController<AController>();

			const bool bHasControllerPairedWithPS = (Controller != nullptr)
													&& (Controller->PlayerState != nullptr) && (
														Controller->PlayerState->GetOwner() == Controller);

			if (!bHasControllerPairedWithPS)
			{
				LOG_ARC_NET(LogPawnExtenstionComponent
					, "UArcHeroComponentBase::CanChangeInitState [CurrentState %s] [DesiredState %s] Controlled does not "
					"PlayerState assigned"
					, *CurrentState.ToString()
					, *DesiredState.ToString())
				return false;
			}
		}

		
		const bool bIsLocallyControlled = Pawn->IsLocallyControlled();
		const bool bIsBot = Pawn->IsBotControlled();

		if (bIsLocallyControlled && !bIsBot)
		{
			AArcCorePlayerController* ArcPC = GetController<AArcCorePlayerController>();

			// The input component and local player is required when locally controlled.
			if (!Pawn->InputComponent || !ArcPC || !ArcPC->GetLocalPlayer())
			{
				LOG_ARC_NET(LogPawnExtenstionComponent
					, "UArcHeroComponentBase::CanChangeInitState [CurrentState %s] [DesiredState %s] Invalid Input "
					"Component"
					, *CurrentState.ToString()
					, *DesiredState.ToString())
				return false;
			}
		}

		LOG_ARC_NET(LogPawnExtenstionComponent
			, "UArcHeroComponentBase::CanChangeInitState [CurrentState %s] [DesiredState %s] Success"
			, *CurrentState.ToString()
			, *DesiredState.ToString())
		return true;
	}
	else if (CurrentState == InitTags.InitState_DataAvailable && DesiredState == InitTags.InitState_DataInitialized)
	{
		// Wait for player state and extension component
		AArcCorePlayerState* ArcPS = GetPlayerState<AArcCorePlayerState>();
		const bool bReached = ArcPS && Manager->HasFeatureReachedInitState(Pawn
								  , UArcPawnExtensionComponent::NAME_ActorFeatureName
								  , InitTags.InitState_DataInitialized);

		//LOG_ARC_NET(LogPawnExtenstionComponent
		//	, "UArcHeroComponentBase::CanChangeInitState [CurrentState %s] [DesiredState %s] Reached InitState [%s]"
		//	, *CurrentState.ToString()
		//	, *DesiredState.ToString()
		//	, bReached ? "TRUE" : "FALSE")

		return bReached;
	}
	else if (CurrentState == InitTags.InitState_DataInitialized && DesiredState == InitTags.InitState_GameplayReady)
	{
		// TODO add ability initialization checks?
		return true;
	}

	LOG_ARC_NET(LogPawnExtenstionComponent
		, "UArcHeroComponentBase::CanChangeInitState [CurrentState %s] [DesiredState %s] FAILED"
		, *CurrentState.ToString()
		, *DesiredState.ToString())
	return false;
}

void UArcHeroComponentBase::HandleChangeInitState(UGameFrameworkComponentManager* Manager
												  , FGameplayTag CurrentState
												  , FGameplayTag DesiredState)
{
	const FArcCoreGameplayTags& InitTags = FArcCoreGameplayTags::Get();
	if (CurrentState == FArcCoreGameplayTags::Get().InitState_DataAvailable && DesiredState ==
		FArcCoreGameplayTags::Get().InitState_DataInitialized)
	{
		APawn* Pawn = GetPawn<APawn>();
		AArcCorePlayerState* ArcPS = GetPlayerState<AArcCorePlayerState>();
		if (!ensure(Pawn && ArcPS))
		{
			LOG_ARC_NET(LogPawnExtenstionComponent
				, "UArcHeroComponentBase::HandleChangeInitState [CurrentState %s] [DesiredState %s] Pawn and PlayerState "
				"invalid"
				, *CurrentState.ToString()
				, *DesiredState.ToString())
			return;
		}

		const bool bIsLocallyControlled = Pawn->IsLocallyControlled();
		const UArcPawnData* PawnData = nullptr;

		if (UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
		{
			LOG_ARC_NET(LogPawnExtenstionComponent
				, "UArcHeroComponentBase::HandleChangeInitState [CurrentState %s] [DesiredState %s] Initialize Ability "
				"System"
				, *CurrentState.ToString()
				, *DesiredState.ToString())
			PawnData = PawnExtComp->GetPawnData<UArcPawnData>();

			// The player state holds the persistent data for this player (state that
			// persists across deaths and multiple pawns). The ability system component
			// and attribute sets live on the player state.
			PawnExtComp->InitializeAbilitySystem(ArcPS->GetArcAbilitySystemComponent()
				, ArcPS);
		}

		ArcPS->SetPawnData(PawnData);

		PawnData->GiveComponents(Pawn);
		
		AArcCorePlayerController* ArcPC = GetController<AArcCorePlayerController>();

		/**
		 * Apply any initialization fragments this pawn might to run.
		 */
		PawnData->RunInitializationFragments(Pawn
			, ArcPS
			, ArcPC);

		/**
		 * Give data fragments, they may require initialized player state.
		 */
		PawnData->GiveDataFragments(Pawn, ArcPS);
		
		if (ArcPC)
		{
			if (Pawn->InputComponent != nullptr)
			{
				InitializePlayerInput(Pawn->InputComponent);
			}
		}
	
		bPawnHasInitialized = true;
		if (UArcPlayerStateExtensionComponent* PSExt = UArcPlayerStateExtensionComponent::Get(ArcPS))
		{
			LOG_ARC_NET(LogPawnExtenstionComponent
				, "UArcHeroComponentBase::HandleChangeInitState [CurrentState %s] [DesiredState %s] Initialize Player "
				"State"
				, *CurrentState.ToString()
				, *DesiredState.ToString())

			/**
			 * Call PlayerStateInitialized, since at this point everything should be available.
			 */
			PSExt->PlayerStateInitialized();
			PSExt->PlayerPawnInitialized(Pawn);
		}

		APlayerController* PC = GetController<APlayerController>();
		if (PC && bIsLocallyControlled)
		{
			AArcHUDBase* Old = Cast<AArcHUDBase>(PC->GetHUD());
			if (PC->GetNetMode() != ENetMode::NM_DedicatedServer && Old == nullptr)
			{
				PC->ClientSetHUD(PawnData->GetHUDClass());
		
				AArcHUDBase* H = PC->GetHUD<AArcHUDBase>();
				if (H)
				{
					LOG_ARC_NET(LogPawnExtenstionComponent
						, "UArcHeroComponentBase::HandleChangeInitState [CurrentState %s] [DesiredState %s] Set HUD "
						"Widgets"
						, *CurrentState.ToString()
						, *DesiredState.ToString())
					H->MakeWidgets();
				}
			}
		}

		ArcPS->GetArcAbilitySystemComponent()->PostAbilitySystemInit();
		
		UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(const_cast<APlayerController*>(PC), NAME_PlayerHeroCharacterInitialized);
		UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(const_cast<APawn*>(Pawn), NAME_PlayerHeroCharacterInitialized);

		UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(const_cast<APlayerController*>(PC), NAME_PlayerHeroPlayerStateInitialized);
		UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(ArcPS, NAME_PlayerHeroPlayerStateInitialized);

		for (TComponentIterator<UArcPawnComponent> It(Pawn); It; ++It)
		{
			It->OnPawnReady();
		}
		
		OnPawnInitialized();
	}
}

void UArcHeroComponentBase::OnActorInitStateChanged(const FActorInitStateChangedParams& Params)
{
	if (Params.FeatureName == UArcPawnExtensionComponent::NAME_ActorFeatureName)
	{
		if (Params.FeatureState == FArcCoreGameplayTags::Get().InitState_DataInitialized)
		{
			// If the extension component says all all other components are initialized,
			// try to progress to next state
			CheckDefaultInitialization();
		}
	}
}

void UArcHeroComponentBase::CheckDefaultInitialization()
{
	const FArcCoreGameplayTags& InitTags = FArcCoreGameplayTags::Get();
	static const TArray<FGameplayTag> StateChain = {
		InitTags.InitState_Spawned
		, InitTags.InitState_DataAvailable
		, InitTags.InitState_DataInitialized
		, InitTags.InitState_GameplayReady
	};

	LOG_ARC_NET(LogPawnExtenstionComponent
		, "UArcHeroComponentBase::CheckDefaultInitialization")

	// This will try to progress from spawned (which is only set in BeginPlay) through the
	// data initialization stages until it gets to gameplay ready
	ContinueInitStateChain(StateChain);
}

void UArcHeroComponentBase::OnRegister()
{
	Super::OnRegister();

	const APawn* Pawn = GetPawnChecked<APawn>();

	if (Pawn == nullptr)
	{
		UE_LOG(LogArcHero
			, Error
			, TEXT( "[ULyraHeroComponent::OnRegister] This component has been added to a blueprint whose base class is "
				"not a Pawn. To use this component, it MUST be placed on a Pawn Blueprint."));

#if WITH_EDITOR
		if (GIsEditor)
		{
			static const FText Message = NSLOCTEXT("LyraHeroComponent"
				, "NotOnPawnError"
				, "has been added to a blueprint whose base class is not a Pawn. To use this component, it MUST be "
				"placed on a Pawn Blueprint. This will cause a crash if you PIE!");
			static const FName HeroMessageLogName = TEXT("LyraHeroComponent");

			FMessageLog(HeroMessageLogName).Error()->AddToken(FUObjectToken::Create(this
				, FText::FromString(GetNameSafe(this))))->AddToken(FTextToken::Create(Message));

			FMessageLog(HeroMessageLogName).Open();
		}
#endif
	}
	else
	{
		// Register with the init state system early, this will only work if this is a
		// game world
		RegisterInitStateFeature();
	}
}

void UArcHeroComponentBase::BeginPlay()
{
	Super::BeginPlay();
	// Listen for when the pawn extension component changes init state
	BindOnActorInitStateChanged(UArcPawnExtensionComponent::NAME_ActorFeatureName
		, FGameplayTag()
		, false);

	// Notifies that we are done spawning, then try the rest of initialization
	ensure(TryToChangeInitState(FArcCoreGameplayTags::Get().InitState_Spawned));
	CheckDefaultInitialization();
}

void UArcHeroComponentBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	const APawn* Pawn = GetPawnChecked<APawn>();

	if (UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
	{
		PawnExtComp->UninitializeAbilitySystem();
	}

	UnregisterInitStateFeature();

	Super::EndPlay(EndPlayReason);
}

void UArcHeroComponentBase::InitializePlayerInput(UInputComponent* PlayerInputComponent)
{
	check(PlayerInputComponent);

	LOG_ARC_NET(LogPawnExtenstionComponent
		, "UArcHeroComponentBase::InitializePlayerInput")

	const APawn* Pawn = GetPawnChecked<APawn>();

	const APlayerController* PC = GetController<APlayerController>();
	check(PC);

	const UArcLocalPlayerBase* LP = Cast<UArcLocalPlayerBase>(PC->GetLocalPlayer());
	check(LP);

	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	check(Subsystem);

	Subsystem->ClearAllMappings();

	if (const UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
	{
		if (const UArcPawnData* PawnData = PawnExtComp->GetPawnData<UArcPawnData>())
		{
			for (const FInputMappingContextAndPriority& Mapping : PawnData->GetAdditionalInputConfigs())
			{
				if (UInputMappingContext* IMC = Mapping.InputMapping.LoadSynchronous())
				{
					if (Mapping.bRegisterWithSettings)
					{
						if (UEnhancedInputUserSettings* Settings = Subsystem->GetUserSettings())
						{
							Settings->RegisterInputMappingContext(IMC);
						}

						FModifyContextOptions Options = {};
						Options.bIgnoreAllPressedKeysUntilRelease = false;
						// Actually add the config to the local player							
						Subsystem->AddMappingContext(IMC, Mapping.Priority, Options);
					}
				}
			}
			
			UArcCoreInputComponent* ArcIC = Pawn->FindComponentByClass<UArcCoreInputComponent>();

			for (UArcInputActionConfig* InputConfig : PawnData->GetInputConfig())
			{
				NativeInitializeArcInput(ArcIC, InputConfig);
				
				TArray<uint32> BindHandles;
				ArcIC->BindCustomInputHandlerActions(InputConfig, BindHandles);
				
				ArcIC->BindAbilityActions(InputConfig
					, this
					, &ThisClass::Input_AbilityInputTagPressed
					, &ThisClass::Input_AbilityInputTagTriggered
					, &ThisClass::Input_AbilityInputTagReleased
					, /*out*/ BindHandles);
			}
		}
	}

	bReadyToBindInputs = true;
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(const_cast<APlayerController*>(PC), NAME_BindInputsNow);
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(const_cast<APawn*>(Pawn), NAME_BindInputsNow);
}

void UArcHeroComponentBase::AddAdditionalInputConfig(const UArcInputActionConfig* InputConfig)
{
	TArray<uint32> BindHandles;

	const APawn* Pawn = GetPawnChecked<APawn>();

	UArcCoreInputComponent* ArcIC = Pawn->FindComponentByClass<UArcCoreInputComponent>();
	check(ArcIC);

	const APlayerController* PC = GetController<APlayerController>();
	check(PC);

	const ULocalPlayer* LP = PC->GetLocalPlayer();
	check(LP);

	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	check(Subsystem);

	if (const UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
	{
		ArcIC->BindCustomInputHandlerActions(InputConfig
			, BindHandles);
		ArcIC->BindAbilityActions(InputConfig
			, this
			, &ThisClass::Input_AbilityInputTagPressed
			, &ThisClass::Input_AbilityInputTagTriggered
			, &ThisClass::Input_AbilityInputTagReleased
			,
			/*out*/ BindHandles);
	}
}

void UArcHeroComponentBase::RemoveAdditionalInputConfig(const UArcInputActionConfig* InputConfig)
{
	//@TODO: Implement me!
}

bool UArcHeroComponentBase::HasPawnInitialized() const
{
	return bPawnHasInitialized;
}

bool UArcHeroComponentBase::IsReadyToBindInputs() const
{
	return bReadyToBindInputs;
}

void UArcHeroComponentBase::Input_AbilityInputTagPressed(FGameplayTag InputTag)
{
	const APawn* Pawn = GetPawnChecked<APawn>();

	if (const UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
	{
		if (UArcCoreAbilitySystemComponent* SfxASC = PawnExtComp->GetArcAbilitySystemComponent())
		{
			SfxASC->AbilityInputTagPressed(InputTag);
		}
	}
}

void UArcHeroComponentBase::Input_AbilityInputTagTriggered(FGameplayTag InputTag)
{
	const APawn* Pawn = GetPawnChecked<APawn>();

	if (const UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
	{
		if (UArcCoreAbilitySystemComponent* SfxASC = PawnExtComp->GetArcAbilitySystemComponent())
		{
			SfxASC->AbilityInputTagTriggered(InputTag);
		}
	}
}

void UArcHeroComponentBase::Input_AbilityInputTagReleased(FGameplayTag InputTag)
{
	const APawn* Pawn = GetPawnChecked<APawn>();

	if (const UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
	{
		if (UArcCoreAbilitySystemComponent* ArcASC = PawnExtComp->GetArcAbilitySystemComponent())
		{
			ArcASC->AbilityInputTagReleased(InputTag);
		}
	}
}