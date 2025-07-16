/**
 * This file is part of ArcX.
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

#include "ArcPawnComponent.h"
#include "Components/GameFrameworkInitStateInterface.h"
#include "CoreMinimal.h"
#include "ArcPawnExtensionComponent.generated.h"

class UArcPawnData;
class UArcCoreAbilitySystemComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogPawnExtenstionComponent
	, Log
	, Log);

/**
 * UArcPawnExtensionComponent
 *
 *	Component used to add functionality to all Pawn classes.
 */
UCLASS()
class ARCCORE_API UArcPawnExtensionComponent
		: public UArcPawnComponent
		, public IGameFrameworkInitStateInterface
{
	GENERATED_BODY()

public:
	UArcPawnExtensionComponent(const FObjectInitializer& ObjectInitializer);

	/** The name of this overall feature, this one depends on the other named component
	 * features */
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

	// Returns the pawn extension component if one exists on the specified actor.
	UFUNCTION(BlueprintPure
		, Category = "Arc Core|Pawn")
	static UArcPawnExtensionComponent* FindPawnExtensionComponent(const AActor* Actor)
	{
		return (Actor ? Actor->FindComponentByClass<UArcPawnExtensionComponent>() : nullptr);
	}

	template <typename T = UArcPawnExtensionComponent>
	static T* Get(const AActor* Actor)
	{
		return Actor->FindComponentByClass<T>();
	}

	template <class T>
	const T* GetPawnData() const
	{
		return Cast<T>(PawnData);
	}

	void SetPawnData(const UArcPawnData* InPawnData);

	UFUNCTION(BlueprintPure
		, Category = "Arc Core|Pawn")
	UArcCoreAbilitySystemComponent* GetArcAbilitySystemComponent() const
	{
		return AbilitySystemComponent;
	}

	// Should be called by the owning pawn to initialize the ability system.
	void InitializeAbilitySystem(UArcCoreAbilitySystemComponent* InASC
								 , AActor* InOwnerActor);

	/*
	 * Should be called by the owning pawn to uninitialize the ability system.
	 * Will stop all abilities and remove all effects nor marked as persistent.
	 */
	virtual void UninitializeAbilitySystem();

	// Should be called by the owning pawn when the pawn's controller changes.
	void HandleControllerChanged();

	// Should be called by the owning pawn when the player state has been replicated.
	void HandlePlayerStateReplicated();

	// Should be called by the owning pawn when the input component is setup.
	void SetupPlayerInputComponent();

	void HandlePlayerStateReplicatedFromController();

	void HandlePawnReplicatedFromController();

	void HandlePawnDataReplicated();
	
	void WaitForExperienceLoad();

	// Call this anytime the pawn needs to check if it's ready to be initialized (pawn
	// data assigned, possessed, etc..). bool CheckPawnReadyToInitialize();

	// Returns true if the pawn is ready to be initialized.
	// UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Arc Core|Pawn", Meta =
	// (ExpandBoolAsExecs = "ReturnValue")) bool IsPawnReadyToInitialize() const { return
	// bPawnReadyToInitialize; }

	// Register with the OnPawnReadyToInitialize delegate and broadcast if condition is
	// already met.
	void OnPawnReadyToInitialize_RegisterAndCall(FSimpleMulticastDelegate::FDelegate Delegate);

	// Register with the OnAbilitySystemInitialized delegate and broadcast if condition is
	// already met.
	void OnAbilitySystemInitialized_RegisterAndCall(FSimpleMulticastDelegate::FDelegate Delegate);

	// Register with the OnAbilitySystemUninitialized delegate.
	void OnAbilitySystemUninitialized_Register(FSimpleMulticastDelegate::FDelegate Delegate);

protected:
	virtual void OnRegister() override;

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnRep_PawnData();

	// Delegate fired when pawn has everything needed for initialization.
	FSimpleMulticastDelegate OnPawnReadyToInitialize;

	// Delegate fired when ability system has been initialized.
	FSimpleMulticastDelegate OnAbilitySystemInitialized;

	// Delegate fired when ability system has been uninitialized.
	FSimpleMulticastDelegate OnAbilitySystemUninitialized;

protected:
	// Pawn data used to create the pawn.  Specified from a spawn function or on a placed
	// instance.
	UPROPERTY(EditInstanceOnly
		, ReplicatedUsing = OnRep_PawnData
		, Category = "Arc Core|Pawn")
	TObjectPtr<const UArcPawnData> PawnData;

	// Pointer to the ability system component that is cached for convenience.
	UPROPERTY()
	TObjectPtr<UArcCoreAbilitySystemComponent> AbilitySystemComponent;

	// True when the pawn has everything needed for initialization.
	bool bPawnReadyToInitialize = false;
};
