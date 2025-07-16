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

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "GameFeatureStateChangeObserver.h"
#include "GameFeaturesProjectPolicies.h"
#include "GameFeaturesSubsystem.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "ArcCoreGameFeaturePolicy.generated.h"

class UGameFeatureData;

/**
 * Manager to keep track of the state machines that bring a game feature plugin into
 * memory and active This class discovers plugins either that are built-in and distributed
 * with the game or are reported externally (i.e. by a web service or other endpoint)
 */
UCLASS(Config = Game)
class ARCCORE_API UArcCoreGameFeaturePolicy : public UDefaultGameFeaturesProjectPolicies
{
	GENERATED_BODY()

public:
	static UArcCoreGameFeaturePolicy& Get();

	UArcCoreGameFeaturePolicy(const FObjectInitializer& ObjectInitializer);

	//~UGameFeaturesProjectPolicies interface
	virtual void InitGameFeatureManager() override;

	virtual void ShutdownGameFeatureManager() override;

	virtual TArray<FPrimaryAssetId> GetPreloadAssetListForGameFeature(const UGameFeatureData* GameFeatureToLoad
																	  , bool bIncludeLoadedAssets = false)
	const override;

	virtual bool IsPluginAllowed(const FString& PluginURL) const override;

	virtual const TArray<FName> GetPreloadBundleStateForGameFeature() const override;

	virtual void GetGameFeatureLoadingMode(bool& bLoadClientData
										   , bool& bLoadServerData) const override;

	//~End of UGameFeaturesProjectPolicies interface

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> Observers;
};

// checked
UCLASS()
class UArcGameFeature_HotfixManager
		: public UObject
		, public IGameFeatureStateChangeObserver
{
	GENERATED_BODY()

public:
	virtual void OnGameFeatureLoading(const UGameFeatureData* GameFeatureData
									  , const FString& PluginURL) override;
};

// checked
UCLASS()
class UArcGameFeature_AddGameplayCuePaths
		: public UObject
		, public IGameFeatureStateChangeObserver
{
	GENERATED_BODY()

public:
	virtual void OnGameFeatureRegistering(const UGameFeatureData* GameFeatureData
										  , const FString& PluginName
										  , const FString& PluginURL) override;

	virtual void OnGameFeatureUnregistering(const UGameFeatureData* GameFeatureData
											, const FString& PluginName
											, const FString& PluginURL) override;
};
