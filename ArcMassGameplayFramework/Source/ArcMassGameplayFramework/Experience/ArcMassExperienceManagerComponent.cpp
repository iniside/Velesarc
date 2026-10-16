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

#include "Experience/ArcMassExperienceManagerComponent.h"

#include "Experience/ArcMassExperienceActionSet.h"
#include "Experience/ArcMassExperienceDefinition.h"
#include "Pawn/ArcMassPawnData.h"

#include "GameFeatureAction.h"
#include "GameFeaturesSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"

UArcMassExperienceManagerComponent::UArcMassExperienceManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void UArcMassExperienceManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, CurrentExperience, SharedParams);
}

void UArcMassExperienceManagerComponent::ServerSetCurrentExperience(FPrimaryAssetId ExperienceId)
{
	// Only the server drives experience selection; clients pick it up via OnRep.
	if (!GetOwner() || GetOwner()->GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	// Forbid mid-game reassignment. Hot-swap would assert in StartExperienceLoad
	// (LoadState != Unloaded) and would also need a deactivate/restart pipeline
	// that doesn't exist yet. OnRep_CurrentExperience therefore only fires once.
	if (LoadState != EArcMassExperienceLoadState::Unloaded)
	{
		UE_LOGF(LogInit, Error
			,"ArcMassExperienceManagerComponent::ServerSetCurrentExperience: experience already loading/loaded (state=%d); ignoring re-assignment to %ls.", static_cast<int32>(LoadState)
			, *ExperienceId.ToString());
		return;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	const FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(ExperienceId);
	if (!AssetPath.IsValid())
	{
		UE_LOGF(LogInit, Error
			,"ArcMassExperienceManagerComponent::ServerSetCurrentExperience: unknown primary asset %ls.", *ExperienceId.ToString());
		return;
	}

	// Take Loading immediately so any further ServerSetCurrentExperience call is rejected
	// by the state guard above while the async definition load is in flight.
	LoadState = EArcMassExperienceLoadState::Loading;

	TWeakObjectPtr<UArcMassExperienceManagerComponent> WeakThis(this);
	AssetManager.GetStreamableManager().RequestAsyncLoad(
		AssetPath,
		FStreamableDelegate::CreateLambda([WeakThis, ExperienceId, AssetPath]()
		{
			UArcMassExperienceManagerComponent* Strong = WeakThis.Get();
			if (Strong == nullptr)
			{
				return;
			}
			Strong->OnExperienceDefinitionLoaded(ExperienceId, AssetPath);
		}));
}

void UArcMassExperienceManagerComponent::OnExperienceDefinitionLoaded(FPrimaryAssetId ExperienceId, FSoftObjectPath AssetPath)
{
	UArcMassExperienceDefinition* Experience = Cast<UArcMassExperienceDefinition>(AssetPath.ResolveObject());
	if (Experience == nullptr)
	{
		UE_LOGF(LogInit, Error
			,"ArcMassExperienceManagerComponent::OnExperienceDefinitionLoaded failed to resolve %ls.", *ExperienceId.ToString());
		LoadState = EArcMassExperienceLoadState::Unloaded;
		return;
	}

	CurrentExperience = Experience;
	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, CurrentExperience, this);
	StartExperienceLoad();
}

void UArcMassExperienceManagerComponent::CallOrRegister_OnExperienceLoaded(FOnArcMassExperienceLoaded::FDelegate&& Delegate)
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

const UArcMassExperienceDefinition* UArcMassExperienceManagerComponent::GetCurrentExperienceChecked() const
{
	check(LoadState == EArcMassExperienceLoadState::Loaded);
	check(CurrentExperience != nullptr);
	return CurrentExperience;
}

const UArcMassExperienceDefinition* UArcMassExperienceManagerComponent::GetCurrentExperienceDefinition() const
{
	return CurrentExperience;
}

bool UArcMassExperienceManagerComponent::IsExperienceLoaded() const
{
	return (LoadState == EArcMassExperienceLoadState::Loaded) && (CurrentExperience != nullptr);
}

void UArcMassExperienceManagerComponent::OnRep_CurrentExperience()
{
	// Clients only kick off the asset/feature/action pipeline once the replicated
	// pointer actually resolved to a valid object.
	if (CurrentExperience != nullptr)
	{
		StartExperienceLoad();
	}
}

void UArcMassExperienceManagerComponent::StartExperienceLoad()
{
	check(CurrentExperience != nullptr);
	// Server side enters here from OnExperienceDefinitionLoaded with state=Loading;
	// client side enters from OnRep_CurrentExperience with state=Unloaded.
	check(LoadState == EArcMassExperienceLoadState::Unloaded
		|| LoadState == EArcMassExperienceLoadState::Loading);

	UE_LOGF(LogInit
		, Log
		,"ARC MASS EXPERIENCE: StartExperienceLoad(CurrentExperience = %ls, %ls)", *CurrentExperience->GetPrimaryAssetId().ToString()
		, GetWorld() ? *GetWorld()->GetName() : TEXT("[no world]"));

	LoadState = EArcMassExperienceLoadState::Loading;

	UAssetManager& AssetManager = UAssetManager::Get();

	TSet<FPrimaryAssetId> BundleAssetList;

	for (const TObjectPtr<UArcMassExperienceActionSet>& ActionSet : CurrentExperience->ActionSets)
	{
		if (ActionSet != nullptr)
		{
			BundleAssetList.Add(ActionSet->GetPrimaryAssetId());
		}
	}
	BundleAssetList.Add(CurrentExperience->GetPrimaryAssetId());

	// Select the asset bundles to stream based on net mode: clients/standalone need the
	// "Client" bundle (which carries UI widget classes registered by GameFeatureActions
	// via AddAdditionalAssetBundleData); server/standalone need "Server". The editor loads
	// both. Without this, the experience's referenced soft assets (e.g. HUD widget classes)
	// are never resident and TSoftClassPtr::Get() returns null.
	const ENetMode NetMode = GetWorld()->GetNetMode();
	const bool bLoadClient = GIsEditor || (NetMode != NM_DedicatedServer);
	const bool bLoadServer = GIsEditor || (NetMode != NM_Client);
	TArray<FName> BundlesToLoad;
	if (bLoadClient)
	{
		BundlesToLoad.Add(FName("Client"));
	}
	if (bLoadServer)
	{
		BundlesToLoad.Add(FName("Server"));
	}

	if (BundleAssetList.Num() == 0)
	{
		OnExperienceLoadComplete();
		return;
	}

	// Pass an empty delegate to ChangeBundleStateForPrimaryAssets and route completion
	// through the handle ourselves. Registering OnExperienceLoadComplete as the manager
	// delegate AND calling it inline on the cache-hit path double-invokes it, tripping
	// the LoadState==Loading check on the second entry.
	TSharedPtr<FStreamableHandle> Handle = AssetManager.ChangeBundleStateForPrimaryAssets(
		BundleAssetList.Array(),
		BundlesToLoad,
		TArray<FName>(),
		false,
		FStreamableDelegate(),
		FStreamableManager::AsyncLoadHighPriority);

	FStreamableDelegate OnComplete = FStreamableDelegate::CreateUObject(this, &ThisClass::OnExperienceLoadComplete);
	if (!Handle.IsValid() || Handle->HasLoadCompleted())
	{
		FStreamableHandle::ExecuteDelegate(OnComplete);
	}
	else
	{
		Handle->BindCompleteDelegate(OnComplete);
		Handle->BindCancelDelegate(FStreamableDelegate::CreateLambda([OnComplete]()
		{
			OnComplete.ExecuteIfBound();
		}));
	}
}

void UArcMassExperienceManagerComponent::OnExperienceLoadComplete()
{
	check(LoadState == EArcMassExperienceLoadState::Loading);
	check(CurrentExperience != nullptr);

	UE_LOGF(LogInit
		, Log
		,"ARC MASS EXPERIENCE: OnExperienceLoadComplete(CurrentExperience = %ls, %ls)", *CurrentExperience->GetPrimaryAssetId().ToString()
		, GetWorld() ? *GetWorld()->GetName() : TEXT("[no world]"));

	if (CurrentExperience->GetDefaultPawnData().IsPending())
	{
		FSoftObjectPath PawnDataPath(CurrentExperience->GetDefaultPawnData().ToSoftObjectPath());
		UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			PawnDataPath,
			FStreamableDelegate::CreateUObject(this, &UArcMassExperienceManagerComponent::OnExperienceLoadComplete));
		return;
	}

	if (bPawnDataBundlesRequested == false)
	{
		bPawnDataBundlesRequested = true;
		UArcMassPawnData* PawnData = CurrentExperience->GetDefaultPawnData().Get();
		if (PawnData != nullptr)
		{
			const FPrimaryAssetId PawnDataId = PawnData->GetPrimaryAssetId();
			if (PawnDataId.IsValid())
			{
				TArray<FName> LoadBundles;
				LoadBundles.Add(FName("Client"));
				LoadBundles.Add(FName("Game"));
				TSharedPtr<FStreamableHandle> BundleHandle = UAssetManager::Get().ChangeBundleStateForPrimaryAssets(
					{ PawnDataId },
					LoadBundles,
					TArray<FName>(),
					false,
					FStreamableDelegate(),
					FStreamableManager::AsyncLoadHighPriority);
				if (BundleHandle.IsValid() && BundleHandle->HasLoadCompleted() == false)
				{
					FStreamableDelegate OnBundleComplete = FStreamableDelegate::CreateUObject(this, &UArcMassExperienceManagerComponent::OnExperienceLoadComplete);
					BundleHandle->BindCompleteDelegate(OnBundleComplete);
					BundleHandle->BindCancelDelegate(FStreamableDelegate::CreateLambda([OnBundleComplete]()
					{
						OnBundleComplete.ExecuteIfBound();
					}));
					return;
				}
			}
		}
	}

	GameFeaturePluginURLs.Reset();

	auto CollectGameFeaturePluginURLs = [This = this](const UPrimaryDataAsset* Context, const TArray<FString>& FeaturePluginList)
	{
		for (const FString& PluginName : FeaturePluginList)
		{
			FString PluginURL;
			if (UGameFeaturesSubsystem::Get().GetPluginURLByName(PluginName, /*out*/ PluginURL))
			{
				This->GameFeaturePluginURLs.AddUnique(PluginURL);
			}
			else
			{
				ensureMsgf(false
					, TEXT("OnExperienceLoadComplete failed to find plugin URL from PluginName %s for experience %s")
					, *PluginName
					, *Context->GetPrimaryAssetId().ToString());
			}
		}
	};

	CollectGameFeaturePluginURLs(CurrentExperience, CurrentExperience->GameFeaturesToEnable);
	for (const TObjectPtr<UArcMassExperienceActionSet>& ActionSet : CurrentExperience->ActionSets)
	{
		if (ActionSet != nullptr)
		{
			CollectGameFeaturePluginURLs(ActionSet, ActionSet->GameFeaturesToEnable);
		}
	}

	NumGameFeaturePluginsLoading = GameFeaturePluginURLs.Num();
	if (NumGameFeaturePluginsLoading > 0)
	{
		LoadState = EArcMassExperienceLoadState::LoadingGameFeatures;
		for (const FString& PluginURL : GameFeaturePluginURLs)
		{
			UGameFeaturesSubsystem::Get().LoadAndActivateGameFeaturePlugin(PluginURL
				, FGameFeaturePluginLoadComplete::CreateUObject(this, &ThisClass::OnGameFeaturePluginLoadComplete));
		}
	}
	else
	{
		OnExperienceFullLoadCompleted();
	}
}

void UArcMassExperienceManagerComponent::OnGameFeaturePluginLoadComplete(const UE::GameFeatures::FResult& Result)
{
	NumGameFeaturePluginsLoading--;

	if (NumGameFeaturePluginsLoading == 0)
	{
		OnExperienceFullLoadCompleted();
	}
}

void UArcMassExperienceManagerComponent::OnExperienceFullLoadCompleted()
{
	check(LoadState != EArcMassExperienceLoadState::Loaded);

	LoadState = EArcMassExperienceLoadState::ExecutingActions;

	FGameFeatureActivatingContext Context;

	const FWorldContext* ExistingWorldContext = GEngine->GetWorldContextFromWorld(GetWorld());
	if (ExistingWorldContext)
	{
		Context.SetRequiredWorldContextHandle(ExistingWorldContext->ContextHandle);
	}

	// CONTRACT: experience-level Actions (CurrentExperience->Actions and
	// ActionSet->Actions) must NOT also be shipped via a GameFeaturePlugin listed in
	// GameFeaturesToEnable. The GFP activation path already invokes
	// OnGameFeatureRegistering/Loading/Activating for actions it owns; calling them
	// again here would double-activate. There is no robust runtime ownership query,
	// so this is enforced by content convention. If you see an action's runtime
	// state appear twice, check that the action isn't authored into a plugin that
	// the same experience also enables.
	auto ActivateListOfActions = [&Context](const TArray<UGameFeatureAction*>& ActionList)
	{
		for (UGameFeatureAction* Action : ActionList)
		{
			if (Action != nullptr)
			{
				Action->OnGameFeatureRegistering();
				Action->OnGameFeatureLoading();
				Action->OnGameFeatureActivating(Context);
			}
		}
	};

	ActivateListOfActions(CurrentExperience->Actions);
	for (const TObjectPtr<UArcMassExperienceActionSet>& ActionSet : CurrentExperience->ActionSets)
	{
		if (ActionSet != nullptr)
		{
			ActivateListOfActions(ActionSet->Actions);
		}
	}

	LoadState = EArcMassExperienceLoadState::Loaded;

	OnExperienceLoaded.Broadcast(CurrentExperience);
	OnExperienceLoaded.Clear();
}

void UArcMassExperienceManagerComponent::OnActionDeactivationCompleted()
{
	check(IsInGameThread());
	++NumObservedPausers;

	// NumExpectedPausers stays INDEX_NONE during the synchronous deactivation
	// iteration in EndPlay; only after iteration finishes does it get the real
	// count. Skip the equality check until then so a synchronous pauser firing
	// inside OnGameFeatureDeactivating cannot trip OnAllActionsDeactivated early.
	// EndPlay does its own final check after assigning NumExpectedPausers.
	if (NumExpectedPausers != INDEX_NONE && NumObservedPausers == NumExpectedPausers)
	{
		OnAllActionsDeactivated();
	}
}

void UArcMassExperienceManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	for (const FString& PluginURL : GameFeaturePluginURLs)
	{
		UGameFeaturesSubsystem::Get().DeactivateGameFeaturePlugin(PluginURL);
	}

	if (LoadState == EArcMassExperienceLoadState::Loaded)
	{
		LoadState = EArcMassExperienceLoadState::Deactivating;

		NumExpectedPausers = INDEX_NONE;
		NumObservedPausers = 0;

		// Pauser callbacks may fire after EndPlay returns and even after the component
		// is destroyed (especially if a GFP teardown is async). Capture a weak ptr.
		TWeakObjectPtr<UArcMassExperienceManagerComponent> WeakThis(this);
		FGameFeatureDeactivatingContext Context(TEXT(""), [WeakThis](FStringView)
		{
			if (UArcMassExperienceManagerComponent* Strong = WeakThis.Get())
			{
				Strong->OnActionDeactivationCompleted();
			}
		});

		const FWorldContext* ExistingWorldContext = GEngine->GetWorldContextFromWorld(GetWorld());
		if (ExistingWorldContext)
		{
			Context.SetRequiredWorldContextHandle(ExistingWorldContext->ContextHandle);
		}

		auto DeactivateListOfActions = [&Context](const TArray<UGameFeatureAction*>& ActionList)
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

		if (CurrentExperience != nullptr)
		{
			DeactivateListOfActions(CurrentExperience->Actions);
			for (const TObjectPtr<UArcMassExperienceActionSet>& ActionSet : CurrentExperience->ActionSets)
			{
				if (ActionSet != nullptr)
				{
					DeactivateListOfActions(ActionSet->Actions);
				}
			}
		}

		NumExpectedPausers = Context.GetNumPausers();

		if (NumExpectedPausers == NumObservedPausers)
		{
			OnAllActionsDeactivated();
		}
	}
}

bool UArcMassExperienceManagerComponent::ShouldShowLoadingScreen(FString& OutReason) const
{
	if (LoadState != EArcMassExperienceLoadState::Loaded)
	{
		OutReason = TEXT("Experience still loading");
		return true;
	}

	return false;
}

void UArcMassExperienceManagerComponent::OnAllActionsDeactivated()
{
	LoadState = EArcMassExperienceLoadState::Unloaded;
	CurrentExperience = nullptr;
	bPawnDataBundlesRequested = false;
}

#if WITH_DEV_AUTOMATION_TESTS
void FArcMassExperienceManagerTestSeam::ForceLoadedAndBroadcast(UArcMassExperienceManagerComponent& Component, const UArcMassExperienceDefinition* Experience)
{
	Component.LoadState = EArcMassExperienceLoadState::Loaded;
	Component.OnExperienceLoaded.Broadcast(Experience);
}
#endif

