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

#include "GameFeatures/ArcCoreGameFeaturePolicy.h"
#include "Engine/Engine.h"
#include "GameFeatureData.h"
#include "GameFeaturesSubsystem.h"
#include "GameFeaturesSubsystemSettings.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

UArcCoreGameFeaturePolicy::UArcCoreGameFeaturePolicy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UArcCoreGameFeaturePolicy& UArcCoreGameFeaturePolicy::Get()
{
	return UGameFeaturesSubsystem::Get().GetPolicy<UArcCoreGameFeaturePolicy>();
}

void UArcCoreGameFeaturePolicy::InitGameFeatureManager()
{
	Observers.Add(NewObject<UArcGameFeature_HotfixManager>());

	UGameFeaturesSubsystem& Subsystem = UGameFeaturesSubsystem::Get();
	for (UObject* Observer : Observers)
	{
		Subsystem.AddObserver(Observer, UGameFeaturesSubsystem::EObserverPluginStateUpdateMode::CurrentAndFuture);
	}

	Super::InitGameFeatureManager();
}

void UArcCoreGameFeaturePolicy::ShutdownGameFeatureManager()
{
	Super::ShutdownGameFeatureManager();

	UGameFeaturesSubsystem& Subsystem = UGameFeaturesSubsystem::Get();
	for (UObject* Observer : Observers)
	{
		Subsystem.RemoveObserver(Observer);
	}
	Observers.Empty();
}

TArray<FPrimaryAssetId> UArcCoreGameFeaturePolicy::GetPreloadAssetListForGameFeature(
	const UGameFeatureData* GameFeatureToLoad
	, bool bIncludeLoadedAssets) const
{
	return Super::GetPreloadAssetListForGameFeature(GameFeatureToLoad
		, bIncludeLoadedAssets);
}

const TArray<FName> UArcCoreGameFeaturePolicy::GetPreloadBundleStateForGameFeature() const
{
	return Super::GetPreloadBundleStateForGameFeature();
}

void UArcCoreGameFeaturePolicy::GetGameFeatureLoadingMode(bool& bLoadClientData
														  , bool& bLoadServerData) const
{
	// Editor will load both, this can cause hitching as the bundles are set to not
	// preload in editor
	bLoadClientData = !IsRunningDedicatedServer();
	bLoadServerData = !IsRunningClientOnly();
}

bool UArcCoreGameFeaturePolicy::IsPluginAllowed(const FString& PluginURL, FString* OutReason) const
{
	return Super::IsPluginAllowed(PluginURL, OutReason);
}

//////////////////////////////////////////////////////////////////////
//

// #include "Hotfix/LyraHotfixManager.h"

void UArcGameFeature_HotfixManager::OnGameFeatureLoading(const UGameFeatureData* GameFeatureData
														 , const FString& PluginURL)
{
	// if (ULyraHotfixManager* HotfixManager =
	// Cast<ULyraHotfixManager>(UOnlineHotfixManager::Get(nullptr)))
	//{
	//	HotfixManager->RequestPatchAssetsFromIniFiles();
	// }
}

