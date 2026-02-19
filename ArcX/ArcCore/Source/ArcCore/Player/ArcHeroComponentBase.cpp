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
#include "AIController.h"
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
#include "MassEntitySubsystem.h"
#include "MoverComponent.h"

#include "PlayerMappableInputConfig.h"
#include "DefaultMovementSet/NavMoverComponent.h"
#include "Engine/World.h"

#include "GameMode/ArcExperienceData.h"
#include "Misc/UObjectToken.h"
#include "Player/ArcLocalPlayerBase.h"
#include "Items/ArcItemDefinition.h"
#include "Kismet/KismetMathLibrary.h"
#include "Mover/ArcMoverTypes.h"

#include "UI/ArcHUDBase.h"
#include "UserSettings/EnhancedInputUserSettings.h"

DEFINE_LOG_CATEGORY(LogArcHero)

UArcCoreMassAgentComponent::UArcCoreMassAgentComponent()
{
}

void UArcCoreMassAgentComponent::SetEntityHandle(const FMassEntityHandle NewHandle)
{
	Super::SetEntityHandle(NewHandle);
	
	if (UArcPawnExtensionComponent* PawnExtensionComponent = GetOwner()->FindComponentByClass<UArcPawnExtensionComponent>())
	{
		PawnExtensionComponent->HandleMassEntityCreated();
	}
};

namespace ArcxHero
{
	static const float LookYawRate = 300.0f;
	static const float LookPitchRate = 165.0f;
}; // namespace ArcxHero

namespace Arcx::Input
{
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Move, "InputTag.Move");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Look_Mouse, "InputTag.Look.Mouse");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Look_Stick, "InputTag.Look.Stick");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Jump, "InputTag.Jump");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Crouch, "InputTag.Crouch");
}

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
		if (UArcCoreMassAgentComponent* MassAgentComponent = Pawn->FindComponentByClass<UArcCoreMassAgentComponent>())
		{
			UMassEntitySubsystem* MassEntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(Pawn->GetWorld());
			FMassEntityManager& EntityManager = MassEntitySubsystem->GetMutableEntityManager();
			if (!EntityManager.IsEntityValid(MassAgentComponent->GetEntityHandle()))
			{
				return false;
			}
		}
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
	
		if (UArcCoreMassAgentComponent* MassAgentComponent = Pawn->FindComponentByClass<UArcCoreMassAgentComponent>())
		{
			UMassEntitySubsystem* MassEntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(Pawn->GetWorld());
			FMassEntityManager& EntityManager = MassEntitySubsystem->GetMutableEntityManager();
			if (FArcCoreAbilitySystemFragment* AbilitySystemFragment = EntityManager.GetFragmentDataPtr<FArcCoreAbilitySystemFragment>(MassAgentComponent->GetEntityHandle()))
			{
				AbilitySystemFragment->AbilitySystem = ArcPS->GetArcAbilitySystemComponent();
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

		TArray<UArcPawnComponent*> PawnComponents;
		GetOwner()->GetComponents<UArcPawnComponent>(PawnComponents);
		for (UArcPawnComponent* PawnComponent : PawnComponents)
		{
			PawnComponent->OnPawnReady();
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

				ArcIC->BindNativeAction(InputConfig, Arcx::Input::InputTag_Move, ETriggerEvent::Started, this, &ThisClass::Input_Move_Started, true);
				ArcIC->BindNativeAction(InputConfig, Arcx::Input::InputTag_Move, ETriggerEvent::Triggered, this, &ThisClass::Input_Move, true);
				ArcIC->BindNativeAction(InputConfig, Arcx::Input::InputTag_Move, ETriggerEvent::Completed, this, &ThisClass::Input_Move_Completed, true);
				ArcIC->BindNativeAction(InputConfig, Arcx::Input::InputTag_Look_Mouse, ETriggerEvent::Triggered, this, &ThisClass::Input_LookMouse, true);
				
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

void UArcHeroComponentBase::Input_Move_Started(const FInputActionValue& InputActionValue)
{
	UE_LOG(LogTemp, Log, TEXT("Input_Move_Started"));
	
	MoveInputVector.X = 0;
	MoveInputVector.Y = 0;
}

void UArcHeroComponentBase::Input_Move(const FInputActionValue& InputActionValue)
{
	APawn* Pawn = GetPawnChecked<APawn>();
	AController* Controller = Pawn->GetController();
	const FVector2D Value = InputActionValue.Get<FVector2D>();
	MoveInputVector.X = Value.X;
	MoveInputVector.Y = Value.Y;
	
	//if (Controller)
	//{	
	//	const FRotator MovementRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
	//
	//	if (Value.X != 0.0f)
	//	{
	//		const FVector MovementDirection = MovementRotation.RotateVector(FVector::RightVector);
	//		
	//		Pawn->AddMovementInput(MovementDirection, Value.X);
	//	}
	//
	//	if (Value.Y != 0.0f)
	//	{
	//		const FVector MovementDirection = MovementRotation.RotateVector(FVector::ForwardVector);
	//		
	//		Pawn->AddMovementInput(MovementDirection, Value.Y);
	//	}
	//}
}

void UArcHeroComponentBase::Input_Move_Completed(const FInputActionValue& InputActionValue)
{
	UE_LOG(LogTemp, Log, TEXT("Input_Move_Completed"));
	MoveInputVector.X = 0;
	MoveInputVector.Y = 0;
}

void UArcHeroComponentBase::Input_LookMouse(const FInputActionValue& InputActionValue)
{
	APawn* Pawn = GetPawnChecked<APawn>();

	const FVector2D Value = InputActionValue.Get<FVector2D>();

	if (Value.X != 0.0f)
	{
		Pawn->AddControllerYawInput(Value.X);
	}

	if (Value.Y != 0.0f)
	{
		Pawn->AddControllerPitchInput(Value.Y);
	}
}

void UArcHeroComponentBase::Input_LookStick(const FInputActionValue& InputActionValue)
{
	APawn* Pawn = GetPawnChecked<APawn>();

	const FVector2D Value = InputActionValue.Get<FVector2D>();

	const UWorld* World = GetWorld();
	check(World);

	if (Value.X != 0.0f)
	{
		//Pawn->AddControllerYawInput(Value.X * ArcHero::LookYawRate * World->GetDeltaSeconds());
	}

	if (Value.Y != 0.0f)
	{
		//Pawn->AddControllerPitchInput(Value.Y * ArcHero::LookPitchRate * World->GetDeltaSeconds());
	}
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

UArcMoverInputProducerComponent::UArcMoverInputProducerComponent(const FObjectInitializer& ObjectInitializer)
	: Super{ObjectInitializer}
{
}

void UArcMoverInputProducerComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	APawn* Pawn = GetPawnChecked<APawn>();
	
	FRotator NewRotation = Pawn->GetControlRotation();
	FRotator AimRot = UKismetMathLibrary::NormalizedDeltaRotator(NewRotation, LastControlRotation);
	ControlRotationRate = AimRot.Yaw / GetWorld()->GetDeltaSeconds();
	LastControlRotation = NewRotation;
}

void UArcMoverInputProducerComponent::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	FCharacterDefaultInputs& CharacterInputs = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();
	FArcMoverCustomInputs& CustomInputs = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FArcMoverCustomInputs>();
	UArcHeroComponentBase* HeroComp = GetHeroComponent();
	APawn* Pawn = GetPawnChecked<APawn>();
	IGameplayTagAssetInterface* GTAI = Cast<IGameplayTagAssetInterface>(Pawn);
	if (GTAI && GTAI->HasMatchingGameplayTag(TAG_Mover_BlockAllInput))
	{
		CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, FVector::ZeroVector);
		return;
	}
	
	CustomInputs.bHaveFocusTarget = false;
	
	if (HeroComp)
	{
		FVector Swizzled;
		Swizzled.X = HeroComp->MoveInputVector.Y;
		Swizzled.Y = HeroComp->MoveInputVector.X;
		Swizzled.Z = 0;
		Swizzled = Swizzled.GetClampedToSize(0, 1);
		
		if (OverrideInput.IsSet())
		{
			Swizzled = OverrideInput.GetValue();
		}
		
		FRotator ControlRotation = Pawn->GetControlRotation();
		ControlRotation.Pitch = 0.0f;
		ControlRotation.Roll = 0.0f;
		
		FVector RotatedVector = UKismetMathLibrary::GreaterGreater_VectorRotator(Swizzled, ControlRotation);
		RotatedVector.Normalize();
		
		CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, RotatedVector);
		
	}
	
	bool bRequestedNavMovement = false;
	if (GetNavMoverComponent())
	{
		bRequestedNavMovement = NavMoverComponent->ConsumeNavMovementData(CachedMoveInputIntent, CachedMoveInputVelocity);
		// Favor velocity input 
		bool bUsingInputIntentForMove = CachedMoveInputVelocity.IsZero();

		if (bUsingInputIntentForMove)
		{
			const FVector FinalDirectionalIntent = CharacterInputs.ControlRotation.RotateVector(CachedMoveInputIntent);
			CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, FinalDirectionalIntent);
		}
		else
		{
			//CharacterInputs.SetMoveInput(EMoveInputType::Velocity, CachedMoveInputVelocity);
			const FVector FinalDirectionalIntent = CharacterInputs.ControlRotation.RotateVector(CachedMoveInputIntent);
			CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, CachedMoveInputVelocity.GetSafeNormal());
			//CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, CachedMoveInputVelocity.GetSafeNormal());
		}

	
		// Normally cached input is cleared by OnMoveCompleted input event but that won't be called if movement came from nav movement
		if (bRequestedNavMovement)
		{
			CachedMoveInputIntent = FVector::ZeroVector;
			CachedMoveInputVelocity = FVector::ZeroVector;
		}
	}
	

	
	{
		CharacterInputs.ControlRotation = Pawn->GetControlRotation();	
	}
	
	{
		CharacterInputs.bIsJumpJustPressed = false;
	}
	
	{
		CustomInputs.RotationMode = EArcMoverAimModeType::Strafe;
		if (AimModeOverride.IsSet())
		{
			CustomInputs.RotationMode = AimModeOverride.GetValue();
		}
	}
	
	if (AAIController* AI = Cast<AAIController>(Pawn->GetController()))
	{
		if (AI->GetFocusActor())
		{
			const FVector FocalPoint = AI->GetFocalPoint();
			const FVector Dir = (FocalPoint - Pawn->GetActorLocation()).GetSafeNormal();
			//CharacterInputs.OrientationIntent = Dir;
			CharacterInputs.ControlRotation.Pitch = 0;
			CharacterInputs.ControlRotation.Roll = 0;
			CharacterInputs.ControlRotation.Yaw = Dir.Rotation().Yaw;
			CustomInputs.bHaveFocusTarget = true;
			CustomInputs.RotationMode = EArcMoverAimModeType::Aim;
		}
	}
	
	{
		FVector Intent = FVector::ZeroVector;
		if (CharacterInputs.GetMoveInput().Size() != 0)
		{
			switch (CustomInputs.RotationMode)
			{
				case EArcMoverAimModeType::OrientToMovement:
					Intent = CharacterInputs.GetMoveInput();
					break;
				case EArcMoverAimModeType::Strafe:
				case EArcMoverAimModeType::Aim:
					FRotator AimRot = CharacterInputs.ControlRotation;
					AimRot.Pitch = 0.0f;
					AimRot.Roll = 0.0f;
					Intent = AimRot.Vector(); 
					break;
			}
		}
		else
		{
			switch (CustomInputs.RotationMode)
			{
				case EArcMoverAimModeType::OrientToMovement:
				case EArcMoverAimModeType::Strafe:
					Intent = CharacterInputs.GetMoveInput();
					break;
				case EArcMoverAimModeType::Aim:
					FRotator AimRot = UKismetMathLibrary::NormalizedDeltaRotator(Pawn->GetActorRotation(), CharacterInputs.ControlRotation);
					if (FMath::Abs(AimRot.Yaw) > 60)
					{
						AimRot.Pitch = 0.0f;
						AimRot.Roll = 0.0f;
						Intent = AimRot.Vector();
					}
					else
					{
						Intent = CharacterInputs.OrientationIntent;
					}
					break;
			}
		}
		CharacterInputs.OrientationIntent = Intent;
	}
	
	{

		
		CustomInputs.Gait = EArcMoverGaitType::Run;
		if (GaitOverride.IsSet())
		{
			CustomInputs.Gait = GaitOverride.GetValue();
		}
		CustomInputs.ControlRotationRate = ControlRotationRate;
		CustomInputs.WantsToCrouch = false;
		
		if (GTAI)
		{
			if (GTAI->HasMatchingGameplayTag(TAG_Mover_Gait_Walk))
			{
				CustomInputs.Gait = EArcMoverGaitType::Walk;
			}
			else if (GTAI->HasMatchingGameplayTag(TAG_Mover_Gait_Run))
			{
				CustomInputs.Gait = EArcMoverGaitType::Run;
			}
			else if (GTAI->HasMatchingGameplayTag(TAG_Mover_Gait_Sprint))
			{
				CustomInputs.Gait = EArcMoverGaitType::Sprint;
			}	
		}
		
	}
	
	{
		if (CustomInputs.RotationMode != EArcMoverAimModeType::OrientToMovement)
		{
			UMoverComponent* Mover = GetOwner()->FindComponentByClass<UMoverComponent>();
			FVector Velocity = Mover->GetVelocity();
			
			Velocity.Z = 0.0f;
			Velocity.Normalize();
			
			// MovementMode
			FVector Forward = Pawn->GetActorForwardVector();
			
			DirectionOfMovement = CharacterInputs.GetMoveInput();
			float Dot = FVector::DotProduct(Forward, DirectionOfMovement);
			CustomInputs.MaxVelocityMultiplier = FMath::GetMappedRangeValueClamped(FVector2f{1.f, -1.f}, {1.f, 0.5f}, Dot);
			if (DirectionOfMovement.Size() <= 0)
			{
				CustomInputs.MovementDirection = EArcMoverDirectionType::F;
				CustomInputs.RotationOffset = 0.f;
			}
			else
			{
				FRotator MoveRot = UKismetMathLibrary::NormalizedDeltaRotator(DirectionOfMovement.Rotation(), CharacterInputs.OrientationIntent.Rotation());
				MovementDirectionAngle = MoveRot.Yaw;
				if (CustomInputs.MovementDirection == EArcMoverDirectionType::F)
				{
					FRotator AimRot = UKismetMathLibrary::NormalizedDeltaRotator(Pawn->GetActorRotation(), CharacterInputs.OrientationIntent.Rotation());
					float Angle = 0;
					if (CustomInputs.RotationOffset <= 0)
					{
						Angle = AimRot.Yaw;
					}
					else
					{
						Angle = CustomInputs.RotationOffset;
					}
					
					if (Angle > 0)
					{
						bool bInRange = UKismetMathLibrary::InRange_FloatFloat(MovementDirectionAngle, -180, -170, true, true);
						if (bInRange)
						{
							MovementDirectionAngle = 179.f;
						}
					}
					else
					{
						bool bInRange = UKismetMathLibrary::InRange_FloatFloat(MovementDirectionAngle, 170, 180, true, true);
						if (bInRange)
						{
							MovementDirectionAngle = -179.f;
						}
					}
				}
				
				FArcMoverRotationThresholds Dirs = GetRotationThresholds(CustomInputs.MovementDirection);
				
				bool bInRangeF  = UKismetMathLibrary::InRange_FloatFloat(MovementDirectionAngle, Dirs.FL, Dirs.FR, true, true);
				
				bool bInRangeLL  = UKismetMathLibrary::InRange_FloatFloat(MovementDirectionAngle, Dirs.BL, Dirs.FL, true, true);
				
				bool bInRangeRL  = UKismetMathLibrary::InRange_FloatFloat(MovementDirectionAngle, Dirs.FR, Dirs.BR, true, true);
				
				if (bInRangeF)
				{
					CustomInputs.MovementDirection = EArcMoverDirectionType::F;
				}
				else if (bInRangeLL)
				{
					CustomInputs.MovementDirection = EArcMoverDirectionType::LL;
				}
				else if (bInRangeRL)
				{
					CustomInputs.MovementDirection = EArcMoverDirectionType::RL;
				}
				else
				{
					CustomInputs.MovementDirection = EArcMoverDirectionType::B;
				}
				
				UCurveFloat* SelectedCurve = nullptr;
				switch (CustomInputs.MovementDirection)
				{
					case EArcMoverDirectionType::F:
						SelectedCurve = RotationOffsetF;
						break;
					case EArcMoverDirectionType::B:
						SelectedCurve = RotationOffsetB;
						break;
					case EArcMoverDirectionType::LL:
						SelectedCurve = RotationOffsetLL;
						break;
					case EArcMoverDirectionType::LR:
						SelectedCurve = RotationOffsetLR;
						break;
					case EArcMoverDirectionType::RL:
						SelectedCurve = RotationOffsetRL;
						break;
					case EArcMoverDirectionType::RR:
						SelectedCurve = RotationOffsetRR;
						break;
				}
		
				if (SelectedCurve)
				{
					CustomInputs.RotationOffset = SelectedCurve->GetFloatValue(MovementDirectionAngle);
				}
			}
		
		}
		else
		{
			CustomInputs.MovementDirection = EArcMoverDirectionType::F;		
		}
		
		
	}
	
	FString Info;
	Info += FString::Printf(TEXT("Move Input X: %.2f Y: %.2f\n"), CharacterInputs.GetMoveInput().X, CharacterInputs.GetMoveInput().Y);
	Info += FString::Printf(TEXT("Control Rotation Rate: %.2f\n"), ControlRotationRate);
	Info += FString::Printf(TEXT("Movement Direction: %s\n"), *UEnum::GetValueAsString(CustomInputs.MovementDirection));
	Info += FString::Printf(TEXT("Movement Direction Angle: %.2f\n"), MovementDirectionAngle);
	Info += FString::Printf(TEXT("Rotation Offset: %.2f\n"), CustomInputs.RotationOffset);
	Info += FString::Printf(TEXT("CustomInputs.MovementDirection %s\n"), *UEnum::GetValueAsString(CustomInputs.MovementDirection));
	Info += FString::Printf(TEXT("Orientation Intent: X: %.2f Y: %.2f\n"), CharacterInputs.OrientationIntent.X, CharacterInputs.OrientationIntent.Y);
	//Info += FString::Printf(TEXT("Orientation Intent: X: %.2f Y: %.2f\n"), CharacterInputs.OrientationIntent.X, CharacterInputs.OrientationIntent.Y);
	
	//DrawDebugString(GetWorld(), Pawn->GetActorLocation() + FVector(0,0,100), Info, nullptr, FColor::White, 0.f, false, 1.5f);
}

UArcHeroComponentBase* UArcMoverInputProducerComponent::GetHeroComponent() const
{
	if (HeroComponent)
	{
		return HeroComponent;
	}
	
	HeroComponent = GetOwner()->FindComponentByClass<UArcHeroComponentBase>();
	return HeroComponent;
}

UNavMoverComponent* UArcMoverInputProducerComponent::GetNavMoverComponent() const
{
	if (NavMoverComponent)
	{
		return NavMoverComponent;
	}
	
	NavMoverComponent = GetOwner()->FindComponentByClass<UNavMoverComponent>();
	return NavMoverComponent;
}
