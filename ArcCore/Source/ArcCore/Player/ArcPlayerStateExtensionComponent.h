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

#include "Components/PlayerStateComponent.h"
#include "CoreMinimal.h"
#include "ArcMacroDefines.h"
#include "Components/GameFrameworkInitStateInterface.h"

#include "ArcPlayerStateExtensionComponent.generated.h"

DECLARE_MULTICAST_DELEGATE(FArcPlayerStateReady);
DECLARE_MULTICAST_DELEGATE_OneParam(FArcPlayerPawnReady
	, APawn* /*InPawn*/);

UCLASS()
class ARCCORE_API UArcPlayerStateExtensionComponent
	: public UPlayerStateComponent
	, public IGameFrameworkInitStateInterface
{
	GENERATED_BODY()

	DEFINE_ARC_DELEGATE(FSimpleMulticastDelegate, OnPlayerStateReady);
	DEFINE_ARC_DELEGATE(FArcPlayerPawnReady, OnPawnReady);

	bool bPlayerStateInitialized = false;
	
public:
	static const FName NAME_PlayerStateInitialized;

	// Sets default values for this component's properties
	UArcPlayerStateExtensionComponent(const FObjectInitializer& ObjectInitializer);

	//~ Begin IGameFrameworkInitStateInterface interface
	virtual FName GetFeatureName() const override
	{
		return TEXT("ArcPlayerStateExtensionComponent");
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
	
	template <typename T = UArcPlayerStateExtensionComponent>
	static T* Get(class AActor* InOwner)
	{
		return InOwner->FindComponentByClass<T>();
	}

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	void PlayerStateInitialized();

	void PlayerPawnInitialized(class APawn* InPawn);
	
	/*
	 * At this point Pawn and Player State Should be available.
	 * Should be called on both clients and server.
	 */
	virtual void HandlePlayerStateReady();

	/**
	 * This is called on both Client (first), and then server (client call server RPC, telling that data has been replicated).
	 */
	virtual void HandlePlayerStateDataReplicated();
};
