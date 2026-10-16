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

#include "Pawn/ArcMassPawnComponent.h"
#include "Components/GameFrameworkInitStateInterface.h"
#include "Engine/StreamableManager.h"
#include "GameplayTagContainer.h"

#include "ArcMassPawnExtensionComponent.generated.h"

class AActor;
class APawn;
class UArcMassPawnData;
class UGameFrameworkComponentManager;
class UInputComponent;
struct FActorInitStateChangedParams;
// CLAUDE.md: FMassEntityManager is a struct.
struct FMassEntityManager;

DECLARE_LOG_CATEGORY_EXTERN(LogArcMassPawnExtensionComponent, Log, Log);

/**
 * UArcMassPawnExtensionComponent
 *
 * Pawn-side feature for Mass-driven pawns. Drives the GameFramework init state machine:
 *
 *   Spawned -> DataAvailable: PawnData assigned, controller possessed (when authority
 *       or locally-controlled), Mass entity handle valid; for locally-controlled
 *       non-bots also requires InputComponent + LocalPlayer + controller paired with
 *       PlayerState.
 *   DataAvailable -> DataInitialized: applies PawnData->Components / InitFragments
 *       and activates the camera rig; for locally-controlled non-bots also wires
 *       Enhanced Input from PawnData->InputConfig. This is the terminal state for
 *       this feature; OnPawnReadyToInitialize fires on entry.
 *
 * Note: GameplayReady is intentionally NOT registered. With a single feature on the
 * pawn the DataInitialized -> GameplayReady step is degenerate.
 *
 * The Mass entity handle arrives via UArcMassFrameworkAgentComponent::OnEntityHandleSet.
 *
 * Input-side responsibilities (lifted from the former UArcMassHeroComponent) are
 * gated to player-controlled non-bot pawns. Ability input is written directly into
 * the pawn entity's FArcAbilityInputFragment (via ArcAbilities::PushInput /
 * ReleaseInput) and signalled to wake the ability StateTree.
 */
UCLASS(ClassGroup = "ArcMass", meta = (BlueprintSpawnableComponent))
class ARCMASSGAMEPLAYFRAMEWORK_API UArcMassPawnExtensionComponent
		: public UArcMassPawnComponent
		, public IGameFrameworkInitStateInterface
{
	GENERATED_BODY()

public:
	UArcMassPawnExtensionComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Feature name used by IGameFrameworkInitStateInterface. */
	static const FName NAME_ActorFeatureName;

	/** Component-extension event broadcast when input bindings have been applied. */
	static const FName NAME_BindInputsNow;

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

	/** Returns the pawn extension component on the specified actor, if any. */
	UFUNCTION(BlueprintPure, Category = "ArcMass|Pawn")
	static UArcMassPawnExtensionComponent* FindPawnExtensionComponent(const AActor* Actor)
	{
		return (Actor ? Actor->FindComponentByClass<UArcMassPawnExtensionComponent>() : nullptr);
	}

	template <typename T = UArcMassPawnExtensionComponent>
	static T* Get(const AActor* Actor)
	{
		return Actor ? Actor->FindComponentByClass<T>() : nullptr;
	}

	template <class T = UArcMassPawnData>
	const T* GetPawnData() const
	{
		return Cast<T>(PawnData);
	}

	/** Server-only: assigns PawnData; replicates to clients. */
	void SetPawnData(const UArcMassPawnData* InPawnData);

	/** Multicast hook fired once when DataAvailable -> DataInitialized completes. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FArcMassOnPawnReadyToInitialize, UArcMassPawnExtensionComponent*);

	FArcMassOnPawnReadyToInitialize& OnPawnReadyToInitialize()
	{
		return OnPawnReadyToInitializeDelegate;
	}

	/** Bind a delegate AND fire it immediately if the pawn is already past DataInitialized. */
	void OnPawnReadyToInitialize_RegisterAndCall(FArcMassOnPawnReadyToInitialize::FDelegate Delegate);

	/**
	 * Called by async-loading InitFragments to register an in-flight FStreamableHandle.
	 * All active handles are cancelled in EndPlay.
	 */
	void TrackInitLoadHandle(TSharedPtr<FStreamableHandle> Handle);

	/**
	 * Called by UArcMassFrameworkAgentComponent::SetEntityHandle once a valid Mass entity
	 * handle has been assigned. Registers a one-shot OnPhaseEnd(PrePhysics) callback so
	 * DataAvailable -> DataInitialized only advances after the entity's creation context
	 * has closed and observer-driven deferred fragments (e.g. sparse FArcMassItemStoreFragment)
	 * have been flushed. Idempotent.
	 */
	void RegisterMassReadyCallback();

	/**
	 * Spawner-driven fast-path counterpart of RegisterMassReadyCallback. When the entity is
	 * created via UArcMassSpawnerSubsystem::SpawnForAgentComponent (FEntityBuilder.Commit ran
	 * synchronously outside Mass processing), observers have already fired and all sparse
	 * fragments are attached at SetEntityHandle time. Skip the PrePhysics one-shot fence and
	 * advance immediately. Idempotent (no-op if bMassEntityFullyInitialized is already true).
	 */
	void NotifyEntityReadyImmediately();

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_PawnData();

protected:
	/** PawnData driving actor-side init. Replicated; push-based. */
	UPROPERTY(EditInstanceOnly, ReplicatedUsing = OnRep_PawnData, Category = "ArcMass|Pawn")
	TObjectPtr<const UArcMassPawnData> PawnData;

	/** True after DataAvailable -> DataInitialized has run once. */
	bool bPawnReadyToInitialize = false;

	/**
	 * Gates DataAvailable -> DataInitialized in CanChangeInitState. Flipped true from
	 * OnMassPrePhysicsEnded once Mass has finished the PrePhysics phase that contains
	 * the entity's creation context — by then observer-deferred sparse fragments are
	 * attached and InitFragments can safely query them.
	 */
	bool bMassEntityFullyInitialized = false;

	/** Handle for the one-shot OnPhaseEnd(PrePhysics) binding; valid only while bound. */
	FDelegateHandle PrePhysicsEndHandle;

	/** Active async-load handles from InitFragments; cancelled and cleared in EndPlay. */
	TArray<TSharedPtr<FStreamableHandle>> PendingInitLoadHandles;

	void OnMassPrePhysicsEnded(float DeltaSeconds);

	FArcMassOnPawnReadyToInitialize OnPawnReadyToInitializeDelegate;
};
