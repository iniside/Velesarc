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


#include "AssetRegistry/AssetData.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameMode/ArcExperienceDefinition.h"
#include "GameMode/ArcCoreWorldSettings.h"
#include "ArcCoreAssetManager.generated.h"

class UArcExperienceData;
class UArcPawnData;

struct FArcBundles
{
	static const FName Equipped;
};

DECLARE_DELEGATE_OneParam(FArcssetManagerStartupJobSubstepProgress
	, float /*NewProgress*/);

/** Handles reporting progress from streamable handles */
struct FArcAssetManagerStartupJob
{
	FArcssetManagerStartupJobSubstepProgress SubstepProgressDelegate;
	TFunction<void(const FArcAssetManagerStartupJob&, TSharedPtr<FStreamableHandle>&)> JobFunc;
	
	FString JobName;
	float JobWeight;
	mutable double LastUpdate = 0;

	/** Simple job that is all synchronous */
	FArcAssetManagerStartupJob(const FString& InJobName
							   , const TFunction<void(const FArcAssetManagerStartupJob&
												      , TSharedPtr<FStreamableHandle>&)>& InJobFunc
							   , float InJobWeight)
		: JobFunc(InJobFunc)
		, JobName(InJobName)
		, JobWeight(InJobWeight)
	{
	}

	/** Perform actual loading, will return a handle if it created one */
	TSharedPtr<FStreamableHandle> DoJob() const;

	void UpdateSubstepProgress(float NewProgress) const
	{
		SubstepProgressDelegate.ExecuteIfBound(NewProgress);
	}

	void UpdateSubstepProgressFromStreamable(TSharedRef<FStreamableHandle> StreamableHandle) const
	{
		if (SubstepProgressDelegate.IsBound())
		{
			// StreamableHandle::GetProgress traverses() a large graph and is quite
			// expensive
			double Now = FPlatformTime::Seconds();
			if (LastUpdate - Now > 1.0 / 60)
			{
				SubstepProgressDelegate.Execute(StreamableHandle->GetProgress());
				LastUpdate = Now;
			}
		}
	}
};

DECLARE_LOG_CATEGORY_EXTERN(LogArcAssetManager
	, Log
	, Log);

/**
 * UArcCoreAssetManager
 *
 *	Game implementation of the asset manager that overrides functionality and stores
 *game-specific types. It is expected that most games will want to override AssetManager
 *as it provides a good place for game-specific loading logic. This class is used by
 *setting 'AssetManagerClassName' in DefaultEngine.ini.
 */
UCLASS(Config = Game)
class ARCCORE_API UArcCoreAssetManager : public UAssetManager
{
	GENERATED_BODY()

public:
	UArcCoreAssetManager();

	// Returns the AssetManager singleton object.
	static UArcCoreAssetManager& Get();

	// Returns the asset referenced by a TSoftObjectPtr.  This will synchronously load the
	// asset if it's not already loaded.
	template <typename AssetType>
	static AssetType* GetAsset(const TSoftObjectPtr<AssetType>& AssetPointer
							   , bool bKeepInMemory = true);

	template <typename AssetType>
	static AssetType* GetAsset(const FPrimaryAssetId& AssetId
							   , bool bKeepInMemory = true);

	template <typename AssetType>
	static const AssetType* GetAssetWithBundles(const FPrimaryAssetId& AssetId
										  , bool bKeepInMemory = true);

	template <typename AssetType>
	static const AssetType* GetAssetWithBundles(UObject* WorldContext
												, const FPrimaryAssetId& AssetId
										  		, bool bKeepInMemory = true);
	
	// Returns the subclass referenced by a TSoftClassPtr.  This will synchronously load
	// the asset if it's not already loaded.
	template <typename AssetType>
	static TSubclassOf<AssetType> GetSubclass(const TSoftClassPtr<AssetType>& AssetPointer
											  , bool bKeepInMemory = true);

	template <typename AssetType>
	static TSubclassOf<AssetType> GetSubclass(const FPrimaryAssetId& AssetId
											  , bool bKeepInMemory = true);

	// Logs all assets currently loaded and tracked by the asset manager.
	static void DumpLoadedAssets();

	template <typename T = UArcExperienceData>
	const T& GetGameData(class UWorld* InWorld) const
	{
		if (AArcCoreWorldSettings* WS = Cast<AArcCoreWorldSettings>(InWorld->GetWorldSettings()))
		{
			FSoftObjectPath AssetPath = UAssetManager::Get().GetPrimaryAssetPath(WS->GetDefaultExperience());
			UArcExperienceDefinition* Experience = Cast<UArcExperienceDefinition>(AssetPath.TryLoad());
			if (Experience && Experience->GetDefaultGameData().IsNull() == false)
			{
				if (T* Ptr = Cast<T>(GetAsset<UArcExperienceData>(Experience->GetDefaultGameData())))
				{
					return *Ptr;
				}
			}
		}
		if (T* Ptr = Cast<T>(GetAsset<UArcExperienceData>(DefaultGameData)))
		{
			return *Ptr;
		}
		checkf(false
			, TEXT("GameData asset [%s] has not been loaded!")
			, *DefaultGameData.ToString());

		// Fatal error above prevents this from being called.
		return *NewObject<T>();
	}

	template <typename T = UArcExperienceDefinition>
	const T* GetGameExperience(class UWorld* InWorld) const
	{
		if (AArcCoreWorldSettings* WS = Cast<AArcCoreWorldSettings>(InWorld->GetWorldSettings()))
		{
			FSoftObjectPath AssetPath = UAssetManager::Get().GetPrimaryAssetPath(WS->GetDefaultExperience());
			T* Experience = Cast<T>(AssetPath.TryLoad());
			if (Experience != nullptr)
			{
				return Experience;
			}
		}
		UE_LOG(LogArcAssetManager
			, Error
			, TEXT("Experience Is Null."));
		// checkf(false, TEXT("GameData asset [%s] has not been loaded!"),
		// *DefaultGameData.ToString());

		// Fatal error above prevents this from being called.
		return nullptr;
	}

	template <typename T = UArcPawnData>
	const T* GetDefaultPawnData() const
	{
		if (T* Ptr = Cast<T>(GetAsset<UArcPawnData>(DefaultPawnData)))
		{
			return Ptr;
		}
		checkf(false
			, TEXT("GameData asset [%s] has not been loaded!")
			, *DefaultPawnData.ToString());

		// Fatal error above prevents this from being called.
		return NewObject<T>();
	}

	TSharedPtr<FStreamableHandle> LoadAsset(FSoftObjectPath& Asset
											, FStreamableDelegate DelegateToCall = FStreamableDelegate()
											, TAsyncLoadPriority Priority = FStreamableManager::DefaultAsyncLoadPriority
											, const FString& DebugName = FStreamableHandle::HandleDebugName_AssetList);

protected:
	static UObject* SynchronousLoadAsset(const FSoftObjectPath& AssetPath);

	static bool ShouldLogAssetLoads();

public:
	// Thread safe way of adding a loaded asset to keep in memory.
	void AddLoadedAsset(const UObject* Asset);

	void RemoveLoadedAsset(const UObject* Asset);

protected:
	virtual void StartInitialLoading() override;

#if WITH_EDITOR
	virtual void PreBeginPIE(bool bStartSimulate) override;
#endif

	void LoadGameData();

	UPrimaryDataAsset* LoadGameDataOfClass(TSubclassOf<UPrimaryDataAsset> DataClass
										   , const TSoftObjectPtr<UPrimaryDataAsset>& DataClassPat
										   , FPrimaryAssetType PrimaryAssetType);

	template <typename GameDataClass>
	const GameDataClass& GetOrLoadTypedGameData(const TSoftObjectPtr<GameDataClass>& DataPath)
	{
		if (const TObjectPtr<UPrimaryDataAsset>* pResult = GameDataMap.Find(GameDataClass::StaticClass()))
		{
			return *CastChecked<GameDataClass>(*pResult);
		}

		// Does a blocking load if needed
		return *CastChecked<const GameDataClass>(LoadGameDataOfClass(GameDataClass::StaticClass()
			, DataPath
			, GameDataClass::StaticClass()->GetFName()));
	}

protected:
	// Global game data asset to use.
	UPROPERTY(Config)
	TSoftObjectPtr<UArcExperienceData> DefaultGameData;

	UPROPERTY(Transient)
	TMap<TObjectPtr<UClass>, TObjectPtr<UPrimaryDataAsset>> GameDataMap;

	// Pawn data used when spawning player pawns if there isn't one set on the player
	// state.
	UPROPERTY(Config)
	TSoftObjectPtr<UArcPawnData> DefaultPawnData;

	UPROPERTY(Config)
	TArray<FName> DefaultGameBundles;

	UPROPERTY(Config)
	TArray<FName> DefaultClientBundles;

	UPROPERTY(Config)
	TArray<FName> DefaultServerBundles;
private:
	void DoAllStartupJobs();

	// Sets up the ability system
	void InitializeAbilitySystem();

	void InitializeGameplayCueManager();

	// Called periodically during loads, could be used to feed the status to a loading
	// screen
	void UpdateInitialGameContentLoadPercent(float GameContentPercent);

	// Ok Im not sure if keeping track of Load References is good idea..
	//  Assets loaded and tracked by the asset manager.
	UPROPERTY()
	TSet<TObjectPtr<const UObject>> LoadedAssets;

	// Used for a scope lock when modifying the list of load assets.
	FCriticalSection LoadedAssetsCritical;

	// The list of tasks to execute on startup. Used to track startup progress.
	TArray<FArcAssetManagerStartupJob> StartupJobs;

	TArray<FName> ExperienceBundles;

public:
	void SetExperienceBundlesSync(UWorld* InWorld, const TArray<FName>& InExperienceBundles);
	void SetExperienceBundlesAsync(UWorld* InWorld, const TArray<FName>& InExperienceBundles);
};

template <typename AssetType>
AssetType* UArcCoreAssetManager::GetAsset(const TSoftObjectPtr<AssetType>& AssetPointer
										  , bool bKeepInMemory)
{
	AssetType* LoadedAsset = nullptr;

	const FSoftObjectPath& AssetPath = AssetPointer.ToSoftObjectPath();

	if (AssetPath.IsValid())
	{
		LoadedAsset = AssetPointer.Get();
		if (!LoadedAsset)
		{
			LoadedAsset = Cast<AssetType>(SynchronousLoadAsset(AssetPath));
			ensureAlwaysMsgf(LoadedAsset
				, TEXT("Failed to load asset [%s]")
				, *AssetPointer.ToString());
		}

		if (LoadedAsset && bKeepInMemory)
		{
			// Added to loaded asset list.
			Get().AddLoadedAsset(Cast<UObject>(LoadedAsset));
		}
	}

	return LoadedAsset;
}

template <typename AssetType>
AssetType* UArcCoreAssetManager::GetAsset(const FPrimaryAssetId& AssetId
										  , bool bKeepInMemory)
{
	AssetType* LoadedAsset = nullptr;

	FAssetData Data;
	Get().GetPrimaryAssetData(AssetId
		, Data);

	LoadedAsset = Cast<AssetType>(Data.GetAsset());

	if (LoadedAsset && bKeepInMemory)
	{
		// Added to loaded asset list.
		Get().AddLoadedAsset(Cast<UObject>(LoadedAsset));
	}

	return LoadedAsset;
}

template <typename AssetType>
const AssetType* UArcCoreAssetManager::GetAssetWithBundles(const FPrimaryAssetId& AssetId
										  				   , bool bKeepInMemory)
{
	AssetType* LoadedAsset = nullptr;

	FAssetData Data;
	Get().GetPrimaryAssetData(AssetId, Data);
	
	LoadedAsset = Cast<AssetType>(Data.GetAsset());

	if (LoadedAsset && bKeepInMemory)
	{
		// Added to loaded asset list.
		Get().AddLoadedAsset(Cast<UObject>(LoadedAsset));
	}

	Get().LoadPrimaryAsset(AssetId, Get().ExperienceBundles);

	return LoadedAsset;
}

template <typename AssetType>
const AssetType* UArcCoreAssetManager::GetAssetWithBundles(UObject* WorldContext
											, const FPrimaryAssetId& AssetId
											, bool bKeepInMemory)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);

	if (World->GetNetMode() == ENetMode::NM_DedicatedServer)
	{
		return GetAssetWithBundles<AssetType>(AssetId);
	}
	
	return GetAssetWithBundles<AssetType>(AssetId);
	
}

template <typename AssetType>
TSubclassOf<AssetType> UArcCoreAssetManager::GetSubclass(const TSoftClassPtr<AssetType>& AssetPointer
														 , bool bKeepInMemory)
{
	TSubclassOf<AssetType> LoadedSubclass;

	const FSoftObjectPath& AssetPath = AssetPointer.ToSoftObjectPath();

	if (AssetPath.IsValid())
	{
		LoadedSubclass = AssetPointer.Get();
		if (!LoadedSubclass)
		{
			LoadedSubclass = Cast<UClass>(SynchronousLoadAsset(AssetPath));
			ensureAlwaysMsgf(LoadedSubclass
				, TEXT("Failed to load asset class [%s]")
				, *AssetPointer.ToString());
		}

		if (LoadedSubclass && bKeepInMemory)
		{
			// Added to loaded asset list.
			Get().AddLoadedAsset(Cast<UObject>(LoadedSubclass));
		}
	}

	return LoadedSubclass;
}

template <typename AssetType>
TSubclassOf<AssetType> UArcCoreAssetManager::GetSubclass(const FPrimaryAssetId& AssetId
														 , bool bKeepInMemory)
{
	TSubclassOf<AssetType> LoadedSubclass;

	FAssetData Data;
	Get().GetPrimaryAssetData(AssetId
		, Data);

	if (Data.IsValid())
	{
		LoadedSubclass = Data.GetClass(EResolveClass::No);

		if (!LoadedSubclass)
		{
			LoadedSubclass = Data.GetClass(EResolveClass::Yes);
			ensureAlwaysMsgf(LoadedSubclass
				, TEXT("Failed to load asset class [%s]")
				, *AssetId.ToString());
		}

		if (LoadedSubclass && bKeepInMemory)
		{
			// Added to loaded asset list.
			Get().AddLoadedAsset(Cast<UObject>(LoadedSubclass));
		}
	}

	return LoadedSubclass;
}
