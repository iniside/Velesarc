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

#include "Components/GameFrameworkInitStateInterface.h"
#include "Components/PlayerStateComponent.h"

#include "ArcMassPlayerStateExtensionComponent.generated.h"

class APawn;
class APlayerState;
class UGameFrameworkComponentManager;
struct FActorInitStateChangedParams;

DECLARE_MULTICAST_DELEGATE(FArcMassPlayerStateReady);

/**
 * UArcMassPlayerStateExtensionComponent
 *
 * Lifecycle-passthrough component on AArcMassPlayerState. Mirrors
 * UArcMassPawnExtensionComponent in shape but contains no GAS surface —
 * it drives the GameFramework init state machine:
 *
 *   Spawned -> DataAvailable: PawnData assigned on the owning PlayerState.
 *   DataAvailable -> DataInitialized: all sibling features on the PS have
 *       reached DataAvailable.
 *   DataInitialized -> GameplayReady: all sibling features have reached
 *       DataInitialized.
 *
 * Consumers waiting on PS-side readiness use IGameFrameworkInitStateInterface's
 * BindOnActorInitStateChanged; the OnPlayerStateReady multicast delegate is
 * preserved for legacy subscribers (e.g. UArcGunStateComponent) that hook a
 * coarse "player state fully initialized" edge.
 */
UCLASS(ClassGroup = (ArcMass), meta = (BlueprintSpawnableComponent))
class ARCMASSGAMEPLAYFRAMEWORK_API UArcMassPlayerStateExtensionComponent
		: public UPlayerStateComponent
		, public IGameFrameworkInitStateInterface
{
	GENERATED_BODY()

public:
	UArcMassPlayerStateExtensionComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Feature name used by IGameFrameworkInitStateInterface. */
	static const FName NAME_ActorFeatureName;

	template <typename T = UArcMassPlayerStateExtensionComponent>
	static T* Get(AActor* InOwner)
	{
		return InOwner ? InOwner->FindComponentByClass<T>() : nullptr;
	}

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

	/** Server- or client-side notification that the owning PlayerState has fully initialized. */
	void BroadcastOnPlayerStateReady();

	FArcMassPlayerStateReady& OnPlayerStateReady()
	{
		return OnPlayerStateReadyDelegate;
	}

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleOwnerPawnSet(APlayerState* Player, APawn* NewPawn, APawn* OldPawn);

	void HandlePawnInitStateChanged(const FActorInitStateChangedParams& Params);

	void BindToPawn(APawn* Pawn);
	void UnbindFromPawn();

	FArcMassPlayerStateReady OnPlayerStateReadyDelegate;

	TWeakObjectPtr<APawn> BoundPawn;
	FDelegateHandle PawnInitStateHandle;

	bool bPlayerStateReadyBroadcasted = false;
};
