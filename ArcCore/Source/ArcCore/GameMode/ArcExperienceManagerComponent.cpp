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

#include "ArcExperienceManagerComponent.h"

#include "ArcCoreGameInstance.h"
#include "ArcExperienceDefinition.h"
#include "GameFeaturesSubsystem.h"
#include "GameMode/ArcCoreGameMode.h"
#include "GameMode/ArcCoreGameState.h"
#include "Core/ArcCoreAssetManager.h"
#include "Pawn/ArcPawnData.h"
#include "Net/UnrealNetwork.h"

#include "ArcExperienceManager.h"
#include "ArcGameDelegates.h"
#include "GameFeatureAction.h"
#include "GameFeaturesSubsystemSettings.h"
#include "TimerManager.h"
#include "Engine/GameInstance.h"
#include "Net/Core/PushModel/PushModel.h"

//@TODO: Async load the experience definition itself
//@TODO: Handle failures explicitly (go into a 'completed but failed' state rather than
// check()-ing)
//@TODO: Do the action phases at the appropriate times instead of all at once
//@TODO: Support deactivating an experience and do the unloading actions
//@TODO: Think about what deactivation/cleanup means for preloaded assets
//@TODO: Handle deactivating game features, right now we 'leak' them enabled
// (for a client moving from experience to experience we actually want to diff the
// requirements and only unload some, not unload everything for them to just be
// immediately reloaded)
//@TODO: Handle both built-in and URL-based plugins (search for colon?)

FString GetClientServerContextString(UObject* ContextObject = nullptr)
{
	ENetRole Role = ROLE_None;

	if (AActor* Actor = Cast<AActor>(ContextObject))
	{
		Role = Actor->GetLocalRole();
	}
	else if (UActorComponent* Component = Cast<UActorComponent>(ContextObject))
	{
		Role = Component->GetOwnerRole();
	}

	if (Role != ROLE_None)
	{
		return (Role == ROLE_Authority) ? TEXT("Server") : TEXT("Client");
	}
	else
	{
#if WITH_EDITOR
		if (GIsEditor)
		{
			extern ENGINE_API FString GPlayInEditorContextString;
			return GPlayInEditorContextString;
		}
#endif
	}

	return TEXT("[]");
}

void UArcExperienceManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass
		, CurrentExperience
		, SharedParams);

	// DOREPLIFETIME(ThisClass, StatTags);
}

namespace ArcxConsoleVariables
{
	static float ExperienceLoadRandomDelayMin = 0.0f;
	static FAutoConsoleVariableRef CVarExperienceLoadRandomDelayMin(TEXT("Arc.chaos.ExperienceDelayLoad.MinSecs")
		, ExperienceLoadRandomDelayMin
		, TEXT(
			"This value (in seconds) will be added as a delay of load completion of the experience (along with the random "
			"value Arc.chaos.ExperienceDelayLoad.RandomSecs)")
		, ECVF_Default);

	static float ExperienceLoadRandomDelayRange = 0.0f;
	static FAutoConsoleVariableRef CVarExperienceLoadRandomDelayRange(TEXT("Arc.chaos.ExperienceDelayLoad.RandomSecs")
		, ExperienceLoadRandomDelayRange
		, TEXT(
			"A random amount of time between 0 and this value (in seconds) will be added as a delay of load completion of "
			"the experience (along with the fixed value Arc.chaos.ExperienceDelayLoad.MinSecs)")
		, ECVF_Default);

	float GetExperienceLoadDelayDuration()
	{
		return FMath::Max(0.0f
			, ExperienceLoadRandomDelayMin + FMath::FRand() * ExperienceLoadRandomDelayRange);
	}
} // namespace ArcxConsoleVariables

UArcExperienceManagerComponent::UArcExperienceManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void UArcExperienceManagerComponent::ServerLoadExperienceDefinition(FPrimaryAssetId ExperienceId)
{
	UArcCoreAssetManager& AssetManager = UArcCoreAssetManager::Get();
	FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(ExperienceId);
	UArcExperienceDefinition* Experience = Cast<UArcExperienceDefinition>(AssetPath.TryLoad());

	CurrentExperience = Experience;
}

void UArcExperienceManagerComponent::ServerSetCurrentExperience(FPrimaryAssetId ExperienceId)
{
	UArcCoreAssetManager& AssetManager = UArcCoreAssetManager::Get();
	FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(ExperienceId);
	UArcExperienceDefinition* Experience = Cast<UArcExperienceDefinition>(AssetPath.TryLoad());

	if (!Experience)
	{
		return;
	}
	
	//check(Experience != nullptr);
	CurrentExperience = Experience;
	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass
		, CurrentExperience
		, this);
	StartExperienceLoad();
}

void UArcExperienceManagerComponent::CallOrRegister_OnExperienceLoaded_HighPriority(
	FOnArcExperienceLoaded::FDelegate&& Delegate)
{
	if (IsExperienceLoaded())
	{
		Delegate.Execute(CurrentExperience);
	}
	else
	{
		OnExperienceLoaded_HighPriority.Add(MoveTemp(Delegate));
	}
}

void UArcExperienceManagerComponent::CallOrRegister_OnExperienceLoaded(FOnArcExperienceLoaded::FDelegate&& Delegate)
{
	if (IsExperienceLoaded())
	{
		Delegate.Execute(CurrentExperience);
	}
	else
	{
		OnExperienceLoaded.Add(MoveTemp(Delegate));
	}
}

void UArcExperienceManagerComponent::CallOrRegister_OnExperienceLoaded_LowPriority(
	FOnArcExperienceLoaded::FDelegate&& Delegate)
{
	if (IsExperienceLoaded())
	{
		Delegate.Execute(CurrentExperience);
	}
	else
	{
		OnExperienceLoaded_LowPriority.Add(MoveTemp(Delegate));
	}
}

const UArcExperienceDefinition* UArcExperienceManagerComponent::GetCurrentExperienceChecked() const
{
	check(LoadState == EArcExperienceLoadState::Loaded);
	check(CurrentExperience != nullptr);
	return CurrentExperience;
}

const UArcExperienceDefinition* UArcExperienceManagerComponent::GetCurrentExperienceDefinition() const
{
	return CurrentExperience;
}

bool UArcExperienceManagerComponent::IsExperienceLoaded() const
{
	return (LoadState == EArcExperienceLoadState::Loaded) && (CurrentExperience != nullptr);
}

void UArcExperienceManagerComponent::OnRep_CurrentExperience()
{
	StartExperienceLoad();
	UArcCoreGameInstance* GameInstance = GetGameInstance<UArcCoreGameInstance>();
	
	//TSoftObjectPtr<UArcExperienceDefinition> SoftCurrentExperience = CurrentExperience;
	//GameInstance->SetCurrentExperience(SoftCurrentExperience);

	UArcCoreAssetManager::Get().SetExperienceBundlesAsync(GetWorld(), CurrentExperience->BundlesState.GetBundles(GetNetMode()));
}

void UArcExperienceManagerComponent::StartExperienceLoad()
{
	check(CurrentExperience != nullptr);
	check(LoadState == EArcExperienceLoadState::Unloaded);

	UE_LOG(LogInit
		, Log
		, TEXT("EXPERIENCE: StartExperienceLoad(CurrentExperience = %s, %s)")
		, *CurrentExperience->GetPrimaryAssetId().ToString()
		, *GetClientServerContextString(this));

	LoadState = EArcExperienceLoadState::Loading;

	UArcCoreAssetManager& AssetManager = UArcCoreAssetManager::Get();

	TSet<FPrimaryAssetId> BundleAssetList;
	TSet<FSoftObjectPath> RawAssetList;

	for (const TObjectPtr<UArcExperienceActionSet>& ActionSet : CurrentExperience->ActionSets)
	{
		if (ActionSet != nullptr)
		{
			BundleAssetList.Add(ActionSet->GetPrimaryAssetId());
		}
	}

	BundleAssetList.Add(CurrentExperience->GetPrimaryAssetId());

	// Load and activate game features associated with the experience

	TArray<FName> BundlesToLoad;
	BundlesToLoad.Add(FArcBundles::Equipped);

	//@TODO: Centralize this client/server stuff into the ArcAssetManager
	const ENetMode OwnerNetMode = GetOwner()->GetNetMode();
	const bool bLoadClient = GIsEditor || (OwnerNetMode != NM_DedicatedServer);
	const bool bLoadServer = GIsEditor || (OwnerNetMode != NM_Client);
	if (bLoadClient)
	{
		BundlesToLoad.Add(UGameFeaturesSubsystemSettings::LoadStateClient);
	}
	if (bLoadServer)
	{
		BundlesToLoad.Add(UGameFeaturesSubsystemSettings::LoadStateServer);
	}

	TSharedPtr<FStreamableHandle> BundleLoadHandle = nullptr;
	if (BundleAssetList.Num() > 0)
	{
		BundleLoadHandle = AssetManager.ChangeBundleStateForPrimaryAssets(
			BundleAssetList.Array()
			, BundlesToLoad
			, {}
			, false
			, FStreamableDelegate()
			, FStreamableManager::AsyncLoadHighPriority);
	}

	TSharedPtr<FStreamableHandle> RawLoadHandle = nullptr;
	if (RawAssetList.Num() > 0)
	{
		RawLoadHandle = AssetManager.LoadAssetList(RawAssetList.Array()
			, FStreamableDelegate()
			, FStreamableManager::AsyncLoadHighPriority
			, TEXT("StartExperienceLoad()"));
	}
	
	// If both async loads are running, combine them
	TSharedPtr<FStreamableHandle> Handle = nullptr;
	if (BundleLoadHandle.IsValid() && RawLoadHandle.IsValid())
	{
		Handle = AssetManager.GetStreamableManager().CreateCombinedHandle({BundleLoadHandle, RawLoadHandle});
	}
	else
	{
		Handle = BundleLoadHandle.IsValid() ? BundleLoadHandle : RawLoadHandle;
	}

	//FStreamableDelegate OnAssetsLoadedDelegate = FStreamableDelegate::CreateUObject(this, &ThisClass::OnExperienceLoadComplete);
	FStreamableDelegate OnAssetsLoadedDelegate = FStreamableDelegate::CreateLambda([](){});
	if (!Handle.IsValid() || Handle->HasLoadCompleted())
	{
		// Assets were already loaded, call the delegate now
		FStreamableHandle::ExecuteDelegate(OnAssetsLoadedDelegate);
	}
	else
	{
		Handle->BindCompleteDelegate(OnAssetsLoadedDelegate);

		Handle->BindCancelDelegate(FStreamableDelegate::CreateLambda([OnAssetsLoadedDelegate]()
		{
			OnAssetsLoadedDelegate.ExecuteIfBound();
		}));
	}

	// This set of assets gets preloaded, but we don't block the start of the experience
	// based on it
	TSet<FPrimaryAssetId> PreloadAssetList;
	//@TODO: Determine assets to preload (but not blocking-ly)
	if (PreloadAssetList.Num() > 0)
	{
		AssetManager.ChangeBundleStateForPrimaryAssets(PreloadAssetList.Array()
			, BundlesToLoad
			, {});
	}

	// Block loading. there is actually not reason to do anything before experience fully loads.
	if (Handle.IsValid())
	{
		Handle->WaitUntilComplete();	
	}
	
	OnExperienceLoadComplete();
}

void UArcExperienceManagerComponent::OnExperienceLoadComplete()
{
	check(LoadState == EArcExperienceLoadState::Loading);
	check(CurrentExperience != nullptr);

	UE_LOG(LogInit, Log, TEXT("EXPERIENCE: OnExperienceLoadComplete(CurrentExperience = %s, %s)"), *CurrentExperience->GetPrimaryAssetId().ToString(), *GetClientServerContextString(this));

	if (CurrentExperience->GetDefaultPawnData().IsPending())
	{
		FSoftObjectPath Path(CurrentExperience->GetDefaultPawnData().ToSoftObjectPath());
		UArcCoreAssetManager::Get().LoadAsset(Path
			, FStreamableDelegate::CreateUObject(this
				, &UArcExperienceManagerComponent::OnExperienceLoadComplete));
		return;
	}
	// find the URLs for our GameFeaturePlugins - filtering out dupes and ones that don't
	// have a valid mapping
	GameFeaturePluginURLs.Reset();
	auto CollectGameFeaturePluginURLs = [This = this] (const UPrimaryDataAsset* Context
													   , const TArray<FString>& FeaturePluginList)
	{
		for (const FString& PluginName : FeaturePluginList)
		{
			FString PluginURL;
			if (UGameFeaturesSubsystem::Get().GetPluginURLByName(PluginName
				, /*out*/ PluginURL))
			{
				This->GameFeaturePluginURLs.AddUnique(PluginURL);
			}
			else
			{
				ensureMsgf(false, TEXT( "OnExperienceLoadComplete failed to find plugin URL from PluginName %s for experience %s - fix data, ignoring for this run"), *PluginName, *Context->GetPrimaryAssetId().ToString());
			}
		}

		// 		// Add in our extra plugin
		// 		if
		// (!CurrentPlaylistData->GameFeaturePluginToActivateUntilDownloadedContentIsPresent.IsEmpty())
		// 		{
		// 			FString PluginURL;
		// 			if
		// (UGameFeaturesSubsystem::Get().GetPluginURLForBuiltInPluginByName(CurrentPlaylistData->GameFeaturePluginToActivateUntilDownloadedContentIsPresent,
		// PluginURL))
		// 			{
		// 				GameFeaturePluginURLs.AddUnique(PluginURL);
		// 			}
		// 		}
	};

	CollectGameFeaturePluginURLs(CurrentExperience, CurrentExperience->GameFeaturesToEnable);
	for (const TObjectPtr<UArcExperienceActionSet>& ActionSet : CurrentExperience->ActionSets)
	{
		if (ActionSet != nullptr)
		{
			CollectGameFeaturePluginURLs(ActionSet
				, ActionSet->GameFeaturesToEnable);
		}
	}

	NumGameFeaturePluginsLoading = GameFeaturePluginURLs.Num();
	if (NumGameFeaturePluginsLoading > 0)
	{
		LoadState = EArcExperienceLoadState::LoadingGameFeatures;
		for (const FString& PluginURL : GameFeaturePluginURLs)
		{
			UGameFeaturesSubsystem::Get().LoadAndActivateGameFeaturePlugin(PluginURL
				, FGameFeaturePluginLoadComplete::CreateUObject(this
					, &ThisClass::OnGameFeaturePluginLoadComplete));
		}
	}
	else
	{
		OnExperienceFullLoadCompleted();
	}
}

void UArcExperienceManagerComponent::OnGameFeaturePluginLoadComplete(const UE::GameFeatures::FResult& Result)
{
	// decrement the number of plugins that are loading
	NumGameFeaturePluginsLoading--;

	if (NumGameFeaturePluginsLoading == 0)
	{
		OnExperienceFullLoadCompleted();
	}
}

void UArcExperienceManagerComponent::OnExperienceFullLoadCompleted()
{
	check(LoadState != EArcExperienceLoadState::Loaded);

	// Insert a random delay for testing (if configured)
	if (LoadState != EArcExperienceLoadState::LoadingChaosTestingDelay)
	{
		const float DelaySecs = ArcxConsoleVariables::GetExperienceLoadDelayDuration();
		if (DelaySecs > 0.0f)
		{
			FTimerHandle DummyHandle;

			LoadState = EArcExperienceLoadState::LoadingChaosTestingDelay;
			GetWorld()->GetTimerManager().SetTimer(DummyHandle
				, this
				, &ThisClass::OnExperienceFullLoadCompleted
				, DelaySecs
				,
				/*bLooping=*/false);

			return;
		}
	}

	LoadState = EArcExperienceLoadState::ExecutingActions;

	// Execute the actions
	FGameFeatureActivatingContext Context;

	// Only apply to our specific world context if set
	const FWorldContext* ExistingWorldContext = GEngine->GetWorldContextFromWorld(GetWorld());
	if (ExistingWorldContext)
	{
		Context.SetRequiredWorldContextHandle(ExistingWorldContext->ContextHandle);
	}

	auto ActivateListOfActions = [&Context] (const TArray<UGameFeatureAction*>& ActionList)
	{
		for (UGameFeatureAction* Action : ActionList)
		{
			if (Action != nullptr)
			{
				//@TODO: The fact that these don't take a world are potentially
				// problematic in client-server PIE
				// The current behavior matches systems like gameplay tags where
				// loading and registering apply to the entire process, but actually
				// applying the results to actors is restricted to a specific world
				Action->OnGameFeatureRegistering();
				Action->OnGameFeatureLoading();
				Action->OnGameFeatureActivating(Context);
			}
		}
	};

	ActivateListOfActions(CurrentExperience->Actions);
	for (const TObjectPtr<UArcExperienceActionSet>& ActionSet : CurrentExperience->ActionSets)
	{
		if (ActionSet != nullptr)
		{
			ActivateListOfActions(ActionSet->Actions);
		}
	}

	LoadState = EArcExperienceLoadState::Loaded;

	OnExperienceLoaded_HighPriority.Broadcast(CurrentExperience);
	OnExperienceLoaded_HighPriority.Clear();

	OnExperienceLoaded.Broadcast(CurrentExperience);
	OnExperienceLoaded.Clear();

	OnExperienceLoaded_LowPriority.Broadcast(CurrentExperience);
	OnExperienceLoaded_LowPriority.Clear();

	UArcGameDelegates* GameDelegates = GetGameInstance<UGameInstance>()->GetSubsystem<UArcGameDelegates>();

	GameDelegates->BroadcastOnExperienceLoaded(CurrentExperience, GetWorld());
}

void UArcExperienceManagerComponent::OnActionDeactivationCompleted()
{
	check(IsInGameThread());
	++NumObservedPausers;

	if (NumObservedPausers == NumExpectedPausers)
	{
		OnAllActionsDeactivated();
	}
}

// void
// UArcExperienceManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>&
// OutLifetimeProps) const
//{
//	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
//
//	DOREPLIFETIME(ThisClass, CurrentExperience);
// }

void UArcExperienceManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// deactivate any features this experience loaded
	//@TODO: This should be handled FILO as well
	for (const FString& PluginURL : GameFeaturePluginURLs)
	{
		UGameFeaturesSubsystem::Get().DeactivateGameFeaturePlugin(PluginURL);
	}

	//@TODO: Ensure proper handling of a partially-loaded state too
	if (LoadState == EArcExperienceLoadState::Loaded)
	{
		LoadState = EArcExperienceLoadState::Deactivating;

		// Make sure we won't complete the transition prematurely if someone registers as
		// a pauser but fires immediately
		NumExpectedPausers = INDEX_NONE;
		NumObservedPausers = 0;

		// Deactivate and unload the actions
		FGameFeatureDeactivatingContext Context(TEXT(""), [this](FStringView) { this->OnActionDeactivationCompleted(); });

		const FWorldContext* ExistingWorldContext = GEngine->GetWorldContextFromWorld(GetWorld());
		if (ExistingWorldContext)
		{
			Context.SetRequiredWorldContextHandle(ExistingWorldContext->ContextHandle);
		}

		auto DeactivateListOfActions = [&Context] (const TArray<UGameFeatureAction*>& ActionList)
		{
			for (UGameFeatureAction* Action : ActionList)
			{
				if (Action)
				{
					Action->OnGameFeatureDeactivating(Context);
					Action->OnGameFeatureUnregistering();
				}
			}
		};

		DeactivateListOfActions(CurrentExperience->Actions);
		for (const TObjectPtr<UArcExperienceActionSet>& ActionSet : CurrentExperience->ActionSets)
		{
			if (ActionSet != nullptr)
			{
				DeactivateListOfActions(ActionSet->Actions);
			}
		}

		NumExpectedPausers = Context.GetNumPausers();

		if (NumExpectedPausers > 0)
		{
			// UE_LOG(LogArcExperience, Error, TEXT("Actions that have asynchronous
			// deactivation aren't fully supported yet in Lyra experiences"));
		}

		if (NumExpectedPausers == NumObservedPausers)
		{
			OnAllActionsDeactivated();
		}
	}
}

bool UArcExperienceManagerComponent::ShouldShowLoadingScreen(FString& OutReason) const
{
	if (LoadState != EArcExperienceLoadState::Loaded)
	{
		OutReason = TEXT("Experience still loading");
		return true;
	}
	else
	{
		return false;
	}
}


void UArcExperienceManagerComponent::OnAllActionsDeactivated()
{
	//@TODO: We actually only deactivated and didn't fully unload...
	LoadState = EArcExperienceLoadState::Unloaded;
	CurrentExperience = nullptr;
	//@TODO:	GEngine->ForceGarbageCollection(true);
}
