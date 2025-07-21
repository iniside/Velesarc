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

#include "ArcCoreAssetManager.h"
#include "AbilitySystem/ArcGameplayCueManager.h"
#include "AbilitySystemGlobals.h"
#include "GameMode/ArcExperienceData.h"
#include "Pawn/ArcPawnData.h"
#include "ArcCoreGameplayTags.h"
#include "ArcEventTags.h"
#include "GameplayTagsManager.h"
#include "Engine/Engine.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopedSlowTask.h"
#include "Stats/StatsMisc.h"

const FName FArcBundles::Equipped("Equipped");

#define STARTUP_JOB_WEIGHTED(JobFunc, JobWeight)          \
	StartupJobs.Add(FArcAssetManagerStartupJob(           \
		#JobFunc,                                         \
		[this](                                           \
			const FArcAssetManagerStartupJob& StartupJob, \
			TSharedPtr<FStreamableHandle>& LoadHandle) {  \
			JobFunc;                                      \
		},                                                \
		JobWeight))
#define STARTUP_JOB(JobFunc) STARTUP_JOB_WEIGHTED(JobFunc, 1.f)

static FAutoConsoleCommand CVarDumpLoadedAssets(TEXT("Arc.DumpLoadedAssets")
	, TEXT("Shows all assets that were loaded via the asset manager and are currently in memory.")
	, FConsoleCommandDelegate::CreateStatic(UArcCoreAssetManager::DumpLoadedAssets));

DEFINE_LOG_CATEGORY(LogArcAssetManager)

TSharedPtr<FStreamableHandle> FArcAssetManagerStartupJob::DoJob() const
{
	const double JobStartTime = FPlatformTime::Seconds();

	TSharedPtr<FStreamableHandle> Handle;
	UE_LOG(LogArcAssetManager, Display, TEXT("Startup job \"%s\" starting"), *JobName);
	JobFunc(*this, Handle);

	if (Handle.IsValid())
	{
		Handle->BindUpdateDelegate(FStreamableUpdateDelegate::CreateRaw(this
			, &FArcAssetManagerStartupJob::UpdateSubstepProgressFromStreamable));
		Handle->WaitUntilComplete(0.0f
			, false);
		Handle->BindUpdateDelegate(FStreamableUpdateDelegate());
	}

	UE_LOG(LogArcAssetManager
		, Display
		, TEXT("Startup job \"%s\" took %.2f seconds to complete")
		, *JobName
		, FPlatformTime::Seconds() - JobStartTime);

	return Handle;
}

UArcCoreAssetManager::UArcCoreAssetManager()
{
	DefaultPawnData = nullptr;
}

UArcCoreAssetManager& UArcCoreAssetManager::Get()
{
	check(GEngine);

	if (UArcCoreAssetManager* Singleton = Cast<UArcCoreAssetManager>(GEngine->AssetManager))
	{
		return *Singleton;
	}

	UE_LOG(LogArcAssetManager
		, Fatal
		, TEXT( "Invalid AssetManagerClassName in DefaultEngine.ini.  It must be set to ArcAssetManager!"));

	// Fatal error above prevents this from being called.
	return *NewObject<UArcCoreAssetManager>();
}

TSharedPtr<FStreamableHandle> UArcCoreAssetManager::LoadAsset(FSoftObjectPath& Asset
															  , FStreamableDelegate DelegateToCall
															  , TAsyncLoadPriority Priority
															  , const FString& DebugName)
{
	TArray<FSoftObjectPath> List;
	List.Add(Asset);
	return LoadAssetList(List
		, DelegateToCall
		, Priority);
}

UObject* UArcCoreAssetManager::SynchronousLoadAsset(const FSoftObjectPath& AssetPath)
{
	if (AssetPath.IsValid())
	{
		TUniquePtr<FScopeLogTime> LogTimePtr;

		if (ShouldLogAssetLoads())
		{
			LogTimePtr = MakeUnique<FScopeLogTime>(*FString::Printf(TEXT("Synchronously loaded asset [%s]")
					, *AssetPath.ToString())
				, nullptr
				, FScopeLogTime::ScopeLog_Seconds);
		}

		if (UAssetManager::IsInitialized())
		{
			return UAssetManager::GetStreamableManager().LoadSynchronous(AssetPath
				, false);
		}

		// Use LoadObject if asset manager isn't ready yet.
		return AssetPath.TryLoad();
	}

	return nullptr;
}

bool UArcCoreAssetManager::ShouldLogAssetLoads()
{
	static bool bLogAssetLoads = FParse::Param(FCommandLine::Get()
		, TEXT("LogAssetLoads"));
	return bLogAssetLoads;
}

void UArcCoreAssetManager::AddLoadedAsset(const UObject* Asset)
{
	if (ensureAlways(Asset))
	{
		FScopeLock LoadedAssetsLock(&LoadedAssetsCritical);
		LoadedAssets.Add(Asset);
	}
}

void UArcCoreAssetManager::RemoveLoadedAsset(const UObject* Asset)
{
	LoadedAssets.Remove(Asset);
	// uint32* Ptr = LoadedAssets.Find(Asset);
	// if(Ptr)
	//{
	//	int32 Num = *Ptr;
	//	if(Num == 1)
	//	{
	//		LoadedAssets.Remove(Asset);
	//		return;
	//	}
	//	else
	//	{
	//		Num = Num - 1;
	//		if(Num <= 0)
	//		{
	//			LoadedAssets.Remove(Asset);
	//			return;
	//		}
	//		LoadedAssets.FindOrAdd(Asset) = Num;
	//	}
	// }
}

void UArcCoreAssetManager::DumpLoadedAssets()
{
	UE_LOG(LogArcAssetManager
		, Log
		, TEXT("========== Start Dumping Loaded Assets =========="));

	for (const UObject* LoadedAsset : Get().LoadedAssets)
	{
		UE_LOG(LogArcAssetManager
			, Log
			, TEXT("  %s")
			, *GetNameSafe(LoadedAsset));
	}

	UE_LOG(LogArcAssetManager
		, Log
		, TEXT("... %d assets in loaded pool")
		, Get().LoadedAssets.Num());
	UE_LOG(LogArcAssetManager
		, Log
		, TEXT("========== Finish Dumping Loaded Assets =========="));
}

void UArcCoreAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();

	LoadGameData();
	STARTUP_JOB(InitializeAbilitySystem());
	STARTUP_JOB(InitializeGameplayCueManager());

	DoAllStartupJobs();
}

void UArcCoreAssetManager::InitializeAbilitySystem()
{
	SCOPED_BOOT_TIMING("UArcCoreAssetManager::InitializeAbilitySystem");

	FArcCoreGameplayTags::InitializeNativeTags();
	UAbilitySystemGlobals::Get().InitGlobalData();
	Arcx::EventTags::InitializeNativeEventTagsBase();

	UAbilitySystemGlobals::Get().InitGlobalData();
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
	// Notify manager that we are done adding native tags.
	Manager.DoneAddingNativeTags();
}

void UArcCoreAssetManager::InitializeGameplayCueManager()
{
	SCOPED_BOOT_TIMING("UArcCoreAssetManager::InitializeGameplayCueManager");

	UArcGameplayCueManager* GCM = UArcGameplayCueManager::Get();
	check(GCM);
	GCM->LoadAlwaysLoadedCues();
}

void UArcCoreAssetManager::LoadGameData()
{
	if (DefaultGameData.Get())
	{
		// GameData has already been loaded.
		return;
	}

	UE_LOG(LogStats
		, Log
		, TEXT("Loading GameData: %s ...")
		, *DefaultGameData.ToString());
	SCOPE_LOG_TIME_IN_SECONDS(TEXT("    ... GameData loaded!")
		, nullptr);

	const UArcExperienceData* LoadedGameData = GetAsset(DefaultGameData);
	if (!LoadedGameData)
	{
		UE_LOG(LogArcAssetManager
			, Error
			, TEXT("Failed to load GameData asset [%s]!")
			, *DefaultGameData.ToString());
	}
}

UPrimaryDataAsset* UArcCoreAssetManager::LoadGameDataOfClass(TSubclassOf<UPrimaryDataAsset> DataClass
															 , const TSoftObjectPtr<UPrimaryDataAsset>& DataClassPath
															 , FPrimaryAssetType PrimaryAssetType)
{
	UPrimaryDataAsset* Asset = nullptr;

	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Loading GameData Object")
		, STAT_GameData
		, STATGROUP_LoadTime);
	if (!DataClassPath.IsNull())
	{
#if WITH_EDITOR
		FScopedSlowTask SlowTask(0
			, FText::Format(NSLOCTEXT("LyraEditor"
					, "BeginLoadingGameDataTask"
					, "Loading GameData {0}")
				, FText::FromName(DataClass->GetFName())));
		const bool bShowCancelButton = false;
		const bool bAllowInPIE = true;
		SlowTask.MakeDialog(bShowCancelButton
			, bAllowInPIE);
#endif
		UE_LOG(LogArcAssetManager
			, Log
			, TEXT("Loading GameData: %s ...")
			, *DataClassPath.ToString());
		SCOPE_LOG_TIME_IN_SECONDS(TEXT("    ... GameData loaded!")
			, nullptr);

		// This can be called recursively in the editor because it is called on demand
		// from PostLoad so force a sync load for primary asset and async load the rest in
		// that case
		if (GIsEditor)
		{
			Asset = DataClassPath.LoadSynchronous();
			LoadPrimaryAssetsWithType(PrimaryAssetType);
		}
		else
		{
			TSharedPtr<FStreamableHandle> Handle = LoadPrimaryAssetsWithType(PrimaryAssetType);
			if (Handle.IsValid())
			{
				Handle->WaitUntilComplete(0.0f
					, false);

				// This should always work
				Asset = Cast<UPrimaryDataAsset>(Handle->GetLoadedAsset());
			}
		}
	}

	if (Asset)
	{
		GameDataMap.Add(DataClass
			, Asset);
	}
	else
	{
		// It is not acceptable to fail to load any GameData asset. It will result in soft
		// failures that are hard to diagnose.
		UE_LOG(LogArcAssetManager
			, Fatal
			, TEXT( "Failed to load GameData asset at %s. Type %s. This is not recoverable and likely means you do not "
				"have the correct data to run %s.")
			, *DataClassPath.ToString()
			, *PrimaryAssetType.ToString()
			, FApp::GetProjectName());
	}

	return Asset;
}

void UArcCoreAssetManager::DoAllStartupJobs()
{
	SCOPED_BOOT_TIMING("ULyraAssetManager::DoAllStartupJobs");
	const double AllStartupJobsStartTime = FPlatformTime::Seconds();

	if (IsRunningDedicatedServer())
	{
		// No need for periodic progress updates, just run the jobs
		for (const FArcAssetManagerStartupJob& StartupJob : StartupJobs)
		{
			StartupJob.DoJob();
		}
	}
	else
	{
		if (StartupJobs.Num() > 0)
		{
			float TotalJobValue = 0.0f;
			for (const FArcAssetManagerStartupJob& StartupJob : StartupJobs)
			{
				TotalJobValue += StartupJob.JobWeight;
			}

			float AccumulatedJobValue = 0.0f;
			for (FArcAssetManagerStartupJob& StartupJob : StartupJobs)
			{
				const float JobValue = StartupJob.JobWeight;
				StartupJob.SubstepProgressDelegate.BindLambda(
					[This = this, AccumulatedJobValue, JobValue, TotalJobValue] (float NewProgress)
					{
						const float SubstepAdjustment = FMath::Clamp(NewProgress
															, 0.0f
															, 1.0f) * JobValue;
						const float OverallPercentWithSubstep =
								(AccumulatedJobValue + SubstepAdjustment) / TotalJobValue;

						This->UpdateInitialGameContentLoadPercent(OverallPercentWithSubstep);
					});

				StartupJob.DoJob();

				StartupJob.SubstepProgressDelegate.Unbind();

				AccumulatedJobValue += JobValue;

				UpdateInitialGameContentLoadPercent(AccumulatedJobValue / TotalJobValue);
			}
		}
		else
		{
			UpdateInitialGameContentLoadPercent(1.0f);
		}
	}

	StartupJobs.Empty();

	UE_LOG(LogArcAssetManager
		, Display
		, TEXT("All startup jobs took %.2f seconds to complete")
		, FPlatformTime::Seconds() - AllStartupJobsStartTime);
}

void UArcCoreAssetManager::UpdateInitialGameContentLoadPercent(float GameContentPercent)
{
	// Could route this to the early startup loading screen
}

void UArcCoreAssetManager::SetExperienceBundlesAsync(UWorld* InWorld, const TArray<FName>& InExperienceBundles)
{
}

void UArcCoreAssetManager::SetExperienceBundlesSync(UWorld* InWorld, const TArray<FName>& InExperienceBundles)
{
	TArray<FName> OldBundles;
	ENetMode NetMode = InWorld->GetNetMode();
	
	if (NetMode == ENetMode::NM_Standalone)
	{
		for (const FName& BundleName : ExperienceBundles)
		{
			if (InExperienceBundles.Contains(BundleName) || DefaultServerBundles.Contains(BundleName)
				|| DefaultClientBundles.Contains(BundleName)
				|| DefaultGameBundles.Contains(BundleName))
			{
				continue;
			}
			OldBundles.AddUnique(BundleName);
		}

		ExperienceBundles.Empty();
		for (const FName& Bundle : DefaultServerBundles)
		{
			ExperienceBundles.AddUnique(Bundle);
		}
		for (const FName& Bundle : DefaultClientBundles)
		{
			ExperienceBundles.AddUnique(Bundle);
		}
	}
	
	if (NetMode == ENetMode::NM_DedicatedServer)
	{
		for (const FName& BundleName : ExperienceBundles)
		{
			if (InExperienceBundles.Contains(BundleName) || DefaultServerBundles.Contains(BundleName)
				|| DefaultGameBundles.Contains(BundleName))
			{
				continue;
			}
			OldBundles.AddUnique(BundleName);
		}
		ExperienceBundles.Empty();
		for (const FName& Bundle : DefaultServerBundles)
		{
			ExperienceBundles.AddUnique(Bundle);
		}
	}
	
	if (NetMode == ENetMode::NM_Client)
	{
		for (const FName& BundleName : ExperienceBundles)
		{
			if (InExperienceBundles.Contains(BundleName) || DefaultClientBundles.Contains(BundleName)
				|| DefaultGameBundles.Contains(BundleName))
			{
				continue;
			}
			OldBundles.AddUnique(BundleName);
		}
		ExperienceBundles.Empty();
		for (const FName& Bundle : DefaultClientBundles)
		{
			ExperienceBundles.AddUnique(Bundle);
		}
	}
	for (const FName& Bundle : InExperienceBundles)
	{
		ExperienceBundles.AddUnique(Bundle);
	}
	for (const FName& Bundle : DefaultGameBundles)
	{
		ExperienceBundles.AddUnique(Bundle);
	}
	TSharedPtr<FStreamableHandle> Handle = ChangeBundleStateForMatchingPrimaryAssets(ExperienceBundles, OldBundles);
	if (Handle.IsValid())
	{
		Handle->WaitUntilComplete();	
	}
}

#if WITH_EDITOR
void UArcCoreAssetManager::PreBeginPIE(bool bStartSimulate)
{
	Super::PreBeginPIE(bStartSimulate);

	{
		FScopedSlowTask SlowTask(0
			, NSLOCTEXT("ArcEditor"
				, "BeginLoadingPIEData"
				, "Loading PIE Data"));
		const bool bShowCancelButton = false;
		const bool bAllowInPIE = true;
		SlowTask.MakeDialog(bShowCancelButton
			, bAllowInPIE);

		// const UArcGameData& LocalGameDataCommon = GetGameData();

		// Intentionally after GetGameData to avoid counting GameData time in this timer
		SCOPE_LOG_TIME_IN_SECONDS(TEXT("PreBeginPIE asset preloading complete")
			, nullptr);

		// You could add preloading of anything else needed for the experience we'll be
		// using here (e.g., by grabbing the default experience from the world settings +
		// the experience override in developer settings)
	}
}
#endif
