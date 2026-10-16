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

#include "Components/GameStateComponent.h"
#include "Engine/AssetManager.h"
#include "GameFeaturePluginOperationResult.h"
#include "LoadingProcessInterface.h"

#include "ArcMassExperienceManagerComponent.generated.h"

class UArcMassExperienceDefinition;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnArcMassExperienceLoaded, const UArcMassExperienceDefinition* /*Experience*/);

enum class EArcMassExperienceLoadState
{
	Unloaded,
	Loading,
	LoadingGameFeatures,
	ExecutingActions,
	Loaded,
	Deactivating
};

/**
 * UArcMassExperienceManagerComponent
 *
 * GameState component that drives experience selection, asset loading, GameFeature
 * plugin activation and action execution. Mass-native counterpart of
 * UArcExperienceManagerComponent with all GAS-specific code paths removed.
 */
UCLASS()
class ARCMASSGAMEPLAYFRAMEWORK_API UArcMassExperienceManagerComponent
	: public UGameStateComponent
	, public ILoadingProcessInterface
{
	GENERATED_BODY()

#if WITH_DEV_AUTOMATION_TESTS
	friend struct ARCMASSGAMEPLAYFRAMEWORK_API FArcMassExperienceManagerTestSeam;
#endif

public:
	UArcMassExperienceManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~UActorComponent interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of UActorComponent interface

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//~ILoadingProcessInterface interface
	virtual bool ShouldShowLoadingScreen(FString& OutReason) const override;
	//~End of ILoadingProcessInterface

	/** Server-only: load and set the current experience, beginning the load pipeline. */
	void ServerSetCurrentExperience(FPrimaryAssetId ExperienceId);

	/** Register/call delegate fired when the experience is fully loaded. */
	void CallOrRegister_OnExperienceLoaded(FOnArcMassExperienceLoaded::FDelegate&& Delegate);

	/** Returns the current experience, asserting if it is not yet fully loaded. */
	const UArcMassExperienceDefinition* GetCurrentExperienceChecked() const;

	/** Returns the current experience definition (may still be mid-load). */
	const UArcMassExperienceDefinition* GetCurrentExperienceDefinition() const;

	/** Returns true if the experience has finished loading (assets, plugins and actions). */
	bool IsExperienceLoaded() const;

private:
	UFUNCTION()
	void OnRep_CurrentExperience();

	void OnExperienceDefinitionLoaded(FPrimaryAssetId ExperienceId, FSoftObjectPath AssetPath);

	void StartExperienceLoad();

	void OnExperienceLoadComplete();

	void OnGameFeaturePluginLoadComplete(const UE::GameFeatures::FResult& Result);

	void OnExperienceFullLoadCompleted();

	void OnActionDeactivationCompleted();

	void OnAllActionsDeactivated();

private:
	UPROPERTY(ReplicatedUsing = OnRep_CurrentExperience)
	TObjectPtr<const UArcMassExperienceDefinition> CurrentExperience;

	EArcMassExperienceLoadState LoadState = EArcMassExperienceLoadState::Unloaded;

	int32 NumGameFeaturePluginsLoading = 0;
	TArray<FString> GameFeaturePluginURLs;

	int32 NumObservedPausers = 0;
	int32 NumExpectedPausers = 0;

	/** True once ChangeBundleStateForPrimaryAssets has been requested for the PawnData
	 *  bundles, preventing a re-entry loop when OnExperienceLoadComplete is called again
	 *  by the bundle-load completion delegate. Reset in OnAllActionsDeactivated. */
	bool bPawnDataBundlesRequested = false;

	/** Delegate called when the experience has finished loading. */
	FOnArcMassExperienceLoaded OnExperienceLoaded;
};

#if WITH_DEV_AUTOMATION_TESTS
/**
 * Test seam: provides minimal access to drive UArcMassExperienceManagerComponent
 * into the Loaded state and broadcast OnExperienceLoaded without requiring the
 * full asset-registry / GameFeature plugin / action pipeline. Intended for unit
 * tests that pin delegate-queueing semantics.
 */
struct ARCMASSGAMEPLAYFRAMEWORK_API FArcMassExperienceManagerTestSeam
{
	/** Flip LoadState to Loaded and broadcast OnExperienceLoaded with the given (possibly null) experience. */
	static void ForceLoadedAndBroadcast(UArcMassExperienceManagerComponent& Component, const UArcMassExperienceDefinition* Experience);
};
#endif
