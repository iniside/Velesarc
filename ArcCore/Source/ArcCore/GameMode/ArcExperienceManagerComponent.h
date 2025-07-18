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

#include "GameFeaturePluginOperationResult.h"
#include "LoadingProcessInterface.h"

#include "ArcExperienceManagerComponent.generated.h"

class UArcExperienceDefinition;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnArcExperienceLoaded, const UArcExperienceDefinition* /*Experience*/);

enum class EArcExperienceLoadState
{
	Unloaded
	, Loading
	, LoadingGameFeatures
	, LoadingChaosTestingDelay
	, ExecutingActions
	, Loaded
	, Deactivating
};

UCLASS()
class ARCCORE_API UArcExperienceManagerComponent
		: public UGameStateComponent
		, public ILoadingProcessInterface
{
	GENERATED_BODY()

public:
	UArcExperienceManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~UActorComponent interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//~End of UActorComponent interface

	//~ILoadingProcessInterface interface
	virtual bool ShouldShowLoadingScreen(FString& OutReason) const override;

	//~End of ILoadingProcessInterface

	void ServerLoadExperienceDefinition(FPrimaryAssetId ExperienceId);
	void ServerSetCurrentExperience(FPrimaryAssetId ExperienceId);

	// Ensures the delegate is called once the experience has been loaded,
	// before others are called.
	// However, if the experience has already loaded, calls the delegate immediately.
	void CallOrRegister_OnExperienceLoaded_HighPriority(FOnArcExperienceLoaded::FDelegate&& Delegate);

	// Ensures the delegate is called once the experience has been loaded
	// If the experience has already loaded, calls the delegate immediately
	void CallOrRegister_OnExperienceLoaded(FOnArcExperienceLoaded::FDelegate&& Delegate);

	// Ensures the delegate is called once the experience has been loaded
	// If the experience has already loaded, calls the delegate immediately
	void CallOrRegister_OnExperienceLoaded_LowPriority(FOnArcExperienceLoaded::FDelegate&& Delegate);

	// This returns the current experience if it is fully loaded, asserting otherwise
	// (i.e., if you called it too soon)
	const UArcExperienceDefinition* GetCurrentExperienceChecked() const;

	/**
	 * Get loaded Experience Definition asset, but does not check if experience fully loaded.
	 * Ie. Game Features and plugins might still be loading.
	 */
	const UArcExperienceDefinition* GetCurrentExperienceDefinition() const;
	
	// Returns true if the experience is fully loaded
	bool IsExperienceLoaded() const;

private:
	UFUNCTION()
	void OnRep_CurrentExperience();

	void StartExperienceLoad();

	void OnExperienceLoadComplete();

	void OnGameFeaturePluginLoadComplete(const UE::GameFeatures::FResult& Result);

	void OnExperienceFullLoadCompleted();

	void OnActionDeactivationCompleted();

	void OnAllActionsDeactivated();

private:
	UPROPERTY(ReplicatedUsing = OnRep_CurrentExperience)
	TObjectPtr<const UArcExperienceDefinition> CurrentExperience;

	EArcExperienceLoadState LoadState = EArcExperienceLoadState::Unloaded;

	int32 NumGameFeaturePluginsLoading = 0;
	TArray<FString> GameFeaturePluginURLs;

	int32 NumObservedPausers = 0;
	int32 NumExpectedPausers = 0;

	/**
	 * Delegate called when the experience has finished loading just before others
	 * (e.g., subsystems that set up for regular gameplay)
	 */
	FOnArcExperienceLoaded OnExperienceLoaded_HighPriority;

	/** Delegate called when the experience has finished loading */
	FOnArcExperienceLoaded OnExperienceLoaded;

	/** Delegate called when the experience has finished loading */
	FOnArcExperienceLoaded OnExperienceLoaded_LowPriority;
};
