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

#include "Pawn/ArcPawnComponent.h"
#include "Components/ActorComponent.h"
#include "Components/GameFrameworkInitStateInterface.h"

#include "ArcMacroDefines.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"
#include "MassAgentComponent.h"
#include "MassCommonFragments.h"
#include "MoverSimulationTypes.h"
#include "NativeGameplayTags.h"
#include "Mover/ArcMoverTypes.h"

#include "ArcHeroComponentBase.generated.h"

class UArcCoreAbilitySystemComponent;
class AArcCorePlayerState;
class UInputComponent;
struct FInputActionValue;
class UArcInputActionConfig;
class UArcCameraMode;
class APlayerState;
class UArcExperienceData;
class UEnhancedInputLocalPlayerSubsystem;

DECLARE_MULTICAST_DELEGATE_OneParam(FArcPlayerStateReady2, class APlayerState*);
DECLARE_DELEGATE_TwoParams(ArcAddDefaultInput, const UArcExperienceData&, UEnhancedInputLocalPlayerSubsystem*);

DECLARE_LOG_CATEGORY_EXTERN(LogArcHero, Log, All);

USTRUCT()
struct FArcCameraModeItem
{
	GENERATED_BODY()

	int32 Priority;
	TSubclassOf<UArcCameraMode> CameraMode;
	UObject* Source = nullptr;
};

USTRUCT()
struct ARCCORE_API FArcCoreAbilitySystemFragment : public FObjectWrapperFragment
{
	GENERATED_BODY()
	
	TWeakObjectPtr<UArcCoreAbilitySystemComponent> AbilitySystem;
};

template<>
struct TMassFragmentTraits<FArcCoreAbilitySystemFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

namespace Arcx::Input
{
	ARCCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Move);
	ARCCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Look_Mouse);
	ARCCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Look_Stick);
	ARCCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Jump);
	ARCCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Crouch);
}

class UCurveFloat;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UArcCoreMassAgentComponent : public UMassAgentComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UArcCoreMassAgentComponent();

	virtual void SetEntityHandle(const FMassEntityHandle NewHandle) override;
};


UCLASS(ClassGroup = (Arc), meta = (BlueprintSpawnableComponent))
class ARCCORE_API UArcHeroComponentBase
		: public UArcPawnComponent
		, public IGameFrameworkInitStateInterface
{
	GENERATED_BODY()

protected:
	// Spec handle for the last ability to set a camera mode.
	FGameplayAbilitySpecHandle AbilityCameraModeOwningSpecHandle;

	TWeakObjectPtr<class UArcItemDefinition> OwningItemCameraMode;

	// True when the pawn has fully finished initialization
	bool bPawnHasInitialized = false;
	
	// True when player input bindings have been applyed, will never be true for
	// non-players
	bool bReadyToBindInputs = false;

	DEFINE_ARC_DELEGATE(FArcPlayerStateReady2, OnPlayerStateReady);

public:
	static const FName NAME_PlayerHeroCharacterInitialized;
	static const FName NAME_PlayerHeroPlayerStateInitialized;

	UArcHeroComponentBase(const FObjectInitializer& ObjectInitializer);

	// Returns the hero component if one exists on the specified actor.
	UFUNCTION(BlueprintPure, Category = "Arc Core|Hero")
	static UArcHeroComponentBase* FindHeroComponent(const AActor* Actor)
	{
		return (Actor ? Actor->FindComponentByClass<UArcHeroComponentBase>() : nullptr);
	}

	void AddAdditionalInputConfig(const UArcInputActionConfig* InputConfig);

	void RemoveAdditionalInputConfig(const UArcInputActionConfig* InputConfig);

	/** True if this has completed OnPawnReadyToInitialize so is prepared for late-added
	 * features */
	bool HasPawnInitialized() const;

	/** True if this player has sent the BindInputsNow event and is prepared for bindings
	 */
	bool IsReadyToBindInputs() const;

	static const FName NAME_BindInputsNow;
	static const FName NAME_ActorFeatureName;
	//~ Begin IGameFrameworkInitStateInterface interface
	virtual FName GetFeatureName() const override
	{
		return NAME_ActorFeatureName;
	}

	virtual bool CanChangeInitState(UGameFrameworkComponentManager* Manager
									, FGameplayTag CurrentState
									, FGameplayTag DesiredState) const override;

	virtual void HandleChangeInitState(UGameFrameworkComponentManager* Manager
									   , FGameplayTag CurrentState
									   , FGameplayTag DesiredState) override;

	virtual void OnActorInitStateChanged(const FActorInitStateChangedParams& Params) override;

	virtual void CheckDefaultInitialization() override;

	//~ End IGameFrameworkInitStateInterface interface

protected:
	virtual void OnRegister() override;

	// virtual bool IsPawnComponentReadyToInitialize() override;
	// virtual void OnPawnReadyToInitialize();

	virtual void OnPawnInitialized()
	{
	};

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void InitializePlayerInput(UInputComponent* PlayerInputComponent);

	// override in child classes if you want to add some game specific input which should
	// be handled directly by this component.
	virtual void NativeInitializeArcInput(class UArcCoreInputComponent* InArcIC
										  , const UArcInputActionConfig* InInputConfig)
	{
	};
	
public:
	FVector2D MoveInputVector = FVector2D::ZeroVector;
	
	void Input_Move_Started(const FInputActionValue& InputActionValue);
	void Input_Move(const FInputActionValue& InputActionValue);
	void Input_Move_Completed(const FInputActionValue& InputActionValue);
	
	void Input_LookMouse(const FInputActionValue& InputActionValue);
	void Input_LookStick(const FInputActionValue& InputActionValue);
	
	void Input_AbilityInputTagPressed(FGameplayTag InputTag);

	void Input_AbilityInputTagTriggered(FGameplayTag InputTag);

	void Input_AbilityInputTagReleased(FGameplayTag InputTag);
};

USTRUCT(BlueprintType)
struct FArcMoverRotationThresholds
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover")
	float FL;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover")
	float FR;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover")
	float BL;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover")
	float BR;
};

class UNavMoverComponent;

UCLASS(ClassGroup = (Arc), meta = (BlueprintSpawnableComponent))
class ARCCORE_API UArcMoverInputProducerComponent
	: public UArcPawnComponent
	, public IMoverInputProducerInterface
{
	GENERATED_BODY()
	
public:
	
	TOptional<bool> bStopImmidietly = false;
	TOptional<FVector> OverrideInput;
	
	UPROPERTY(EditAnywhere)
	TOptional<EArcMoverGaitType> GaitOverride;
	
	UPROPERTY(EditAnywhere)
	TOptional<EArcMoverAimModeType> AimModeOverride;
	
	float ControlRotationRate = 0;
	FRotator LastControlRotation = FRotator::ZeroRotator;
	FVector DirectionOfMovement = FVector::ZeroVector;
	float MovementDirectionAngle = 0;
	
	FVector CachedMoveInputIntent = FVector::ZeroVector;
	FVector CachedMoveInputVelocity = FVector::ZeroVector;
	
	UPROPERTY(EditAnywhere)
	int32 RotationModeType = 0;
	
	UPROPERTY()
	mutable TObjectPtr<UArcHeroComponentBase> HeroComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Nav Movement")
	mutable TObjectPtr<UNavMoverComponent> NavMoverComponent;
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<UCurveFloat> RotationOffsetF;
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<UCurveFloat> RotationOffsetB;
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<UCurveFloat> RotationOffsetLL;
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<UCurveFloat> RotationOffsetLR;
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<UCurveFloat> RotationOffsetRL;
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<UCurveFloat> RotationOffsetRR;
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<UCurveFloat> RotationOffsetSlide;
	
	UArcMoverInputProducerComponent(const FObjectInitializer& ObjectInitializer);
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override;
	
	UArcHeroComponentBase* GetHeroComponent() const;
	UNavMoverComponent* GetNavMoverComponent() const;
	
	FArcMoverRotationThresholds GetRotationThresholds(EArcMoverDirectionType Type) const
	{
		FArcMoverRotationThresholds Value;
		
		if (RotationModeType == 0)
		{
			switch (Type)
			{
				case EArcMoverDirectionType::F:
				case EArcMoverDirectionType::B:
					Value.FL = -60.f;
					Value.FR = 60.f;
					Value.BL = -120.f;
					Value.BR = 120.f;
					break;
				case EArcMoverDirectionType::LL:
				case EArcMoverDirectionType::LR:
				case EArcMoverDirectionType::RL:
				case EArcMoverDirectionType::RR:
					Value.FL = -40.f;
					Value.FR = 40.f;
					Value.BL = -140.f;
					Value.BR = 140.f;
					break;
			}
		}
		else if (RotationModeType == 1)
		{
			switch (Type)
			{
				case EArcMoverDirectionType::B:
					Value.FL = -120.f;
					Value.FR = 120.f;
					Value.BL = -140.f;
					Value.BR = 140.f;
					break;
				case EArcMoverDirectionType::F:
				case EArcMoverDirectionType::LL:
				case EArcMoverDirectionType::LR:
				case EArcMoverDirectionType::RL:
				case EArcMoverDirectionType::RR:
					Value.FL = -120.f;
					Value.FR = 120.f;
					Value.BL = -120.f;
					Value.BR = 120.f;
					break;
			}
		}
		return Value;
	}
};