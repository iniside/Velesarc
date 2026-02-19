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


#include "AbilitySystemInterface.h"
#include "GameplayTagAssetInterface.h"
#include "MoverSimulationTypes.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystem/ArcAbilitySet.h"
#include "Equipment/ArcSkeletalMeshOwnerInterface.h"
#include "Perception/AIPerceptionListenerInterface.h"
#include "ArcCorePawn.generated.h"

class IGameplayCameraSystemHost;
class UGameplayAbility;
class UGameplayEffect;
class UArcAttributeSet;
class UArcCoreAbilitySystemComponent;
class UCharacterMoverComponent;
class UNavMoverComponent;

UCLASS(Abstract)
class ARCCORE_API AArcCorePawn : public APawn, public IMoverInputProducerInterface
{
	GENERATED_BODY()
	
	
public:
	// Sets default values for this pawn's properties
	AArcCorePawn(const FObjectInitializer& ObjectInitializer);

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void BeginPlay() override;

	virtual void PostInitializeComponents() override;

	// Accessor for the actor's movement component
	UFUNCTION(BlueprintPure, Category = Mover)
	UCharacterMoverComponent* GetMoverComponent() const { return CharacterMotionComponent; }

	// Request the character starts moving with an intended directional magnitude. A length of 1 indicates maximum acceleration.
	UFUNCTION(BlueprintCallable, Category=MoverExamples)
	virtual void RequestMoveByIntent(const FVector& DesiredIntent) { CachedMoveInputIntent = DesiredIntent; }

	// Request the character starts moving with a desired velocity. This will be used in lieu of any other input.
	UFUNCTION(BlueprintCallable, Category=MoverExamples)
	virtual void RequestMoveByVelocity(const FVector& DesiredVelocity) { CachedMoveInputVelocity=DesiredVelocity; }

	//~ Begin INavAgentInterface Interface
	virtual FVector GetNavAgentLocation() const override;
	//~ End INavAgentInterface Interface
	
	virtual void UpdateNavigationRelevance() override;
	
protected:
	// Entry point for input production. Do not override. To extend in derived character types, override OnProduceInput for native types or implement "Produce Input" blueprint event
	virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override;

	// Override this function in native class to author input for the next simulation frame. Consider also calling Super method.
	virtual void OnProduceInput(float DeltaMs, FMoverInputCmdContext& InputCmdResult);

	// Implement this event in Blueprints to author input for the next simulation frame. Consider also calling Parent event.
	UFUNCTION(BlueprintImplementableEvent, DisplayName="On Produce Input", meta = (ScriptName = "OnProduceInput"))
	FMoverInputCmdContext OnProduceInputInBlueprint(float DeltaMs, FMoverInputCmdContext InputCmd);


public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MoverExamples)
	bool bNativeProduceInput = true;
	
	// Whether or not we author our movement inputs relative to whatever base we're standing on, or leave them in world space. Only applies if standing on a base of some sort.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MoverExamples)
	bool bUseBaseRelativeMovement = true;
	
	/**
	 * If true, rotate the Character toward the direction the actor is moving
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MoverExamples)
	bool bOrientRotationToMovement = true;
	
	/**
	 * If true, the actor will continue orienting towards the last intended orientation (from input) even after movement intent input has ceased.
	 * This makes the character finish orienting after a quick stick flick from the player.  If false, character will not turn without input.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MoverExamples)
	bool bMaintainLastInputOrientation = false;

protected:
	UPROPERTY(Category = Movement, VisibleAnywhere, BlueprintReadOnly, Transient, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCharacterMoverComponent> CharacterMotionComponent;

	/** Holds functionality for nav movement data and functions */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Nav Movement")
	TObjectPtr<UNavMoverComponent> NavMoverComponent;
	
private:
	FVector LastAffirmativeMoveInput = FVector::ZeroVector;	// Movement input (intent or velocity) the last time we had one that wasn't zero

	FVector CachedMoveInputIntent = FVector::ZeroVector;
	FVector CachedMoveInputVelocity = FVector::ZeroVector;

	FRotator CachedTurnInput = FRotator::ZeroRotator;
	FRotator CachedLookInput = FRotator::ZeroRotator;

	bool bIsJumpJustPressed = false;
	bool bIsJumpPressed = false;
	bool bIsFlyingActive = false;
	bool bShouldToggleFlying = false;


	uint8 bHasProduceInputinBpFunc : 1;
};

UCLASS(Abstract)
class ARCCORE_API AArcCorePawnAbilitySystem : public AArcCorePawn, public IAbilitySystemInterface, public IGameplayTagAssetInterface, public IAIPerceptionListenerInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AArcCorePawnAbilitySystem(const FObjectInitializer& ObjectInitializer);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ability System")
	TObjectPtr<UArcCoreAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability System")
	TObjectPtr<UArcAbilitySet> AbilitySet;
	
public:
	virtual void BeginPlay() override;
	virtual void OnProduceInput(float DeltaMs, FMoverInputCmdContext& InputCmdResult) override;
	
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
	
	virtual UAIPerceptionComponent* GetPerceptionComponent() override;
};

class UGameplayCameraComponent;
class UCapsuleComponent;

UCLASS()
class ARCCORE_API AArcCorePlayerPawn : public APawn
	, public IAbilitySystemInterface
	, public IGameplayTagAssetInterface
	, public IAIPerceptionListenerInterface
	, public IArcSkeletalMeshOwnerInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AArcCorePlayerPawn(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(Replicated)
	FVector LocalViewPoint = FVector::ZeroVector;

	UPROPERTY(Replicated)
	FRotator LocalViewRotation = FRotator::ZeroRotator;
	
protected:
	UPROPERTY()
	mutable TObjectPtr<UCapsuleComponent> CachedCapsuleComponent;
	
	UPROPERTY()
	mutable TObjectPtr<UGameplayCameraComponent> CachedGameplayCameraComponent;
	
	UPROPERTY()
	mutable TObjectPtr<UArcCoreAbilitySystemComponent> AbilitySystemComponent2;
		
	IGameplayCameraSystemHost* GameplayCameraSystemHost;
	
public:
	virtual void PossessedBy(AController* NewController) override;

	virtual void UnPossessed() override;

	virtual void OnRep_Controller() override;

	virtual void OnRep_PlayerState() override;
	
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	
	virtual void GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const override;
	
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
	
	virtual UAIPerceptionComponent* GetPerceptionComponent() override;
	
	UCapsuleComponent* GetCapsuleComponent() const;

	UGameplayCameraComponent* GetGameplayCameraComponent() const;

	virtual USkeletalMeshComponent* GetSkeletalMesh() const override
	{
		return nullptr;
	}
};

UINTERFACE(BlueprintType, Blueprintable)
class ARCCORE_API UArcCoreFloorLocationInterface : public UInterface
{
	GENERATED_BODY()
};

class IArcCoreFloorLocationInterface
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ArcCore")
	FVector ArcGetFloorLocation() const;
	
	virtual FVector ArcGetFloorLocation_Implementation() const { return FVector::ZeroVector; }
};