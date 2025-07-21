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

#include "GameplayCueManager.h"
#include "Tickable.h"
#include "ArcGameplayCueManager.generated.h"

USTRUCT()
struct ARCCORE_API FArcGCMTickFunction : public FTickFunction
{
	GENERATED_BODY()

public:
	friend class UArcGameplayCueManager;

	FArcGCMTickFunction()
	{
	}

	// Begin FTickFunction overrides
	virtual void ExecuteTick(float DeltaTime
							 , enum ELevelTick TickType
							 , ENamedThreads::Type CurrentThread
							 , const FGraphEventRef& MyCompletionGraphEvent) override;

	virtual FString DiagnosticMessage() override
	{
		return FString();
	};

	virtual FName DiagnosticContext(bool bDetailed) override
	{
		return "AuGCMTickFunction";
	};

	virtual FName GetSystemName() const
	{
		return NAME_None;
	}

	// End FTickFunction overrides

protected:
	class UArcGameplayCueManager* CueManager = nullptr;
};

template <>
struct TStructOpsTypeTraits<FArcGCMTickFunction> : public TStructOpsTypeTraitsBase2<FArcGCMTickFunction>
{
	enum
	{
		WithCopy = false
	};
};

DECLARE_LOG_CATEGORY_EXTERN(LogArcGameplayCue
	, Log
	, Log);

UCLASS()
class ARCCORE_API UArcGameplayCueManager
		: public UGameplayCueManager
		, public FTickableGameObject
{
	GENERATED_BODY()

private:
	FArcGCMTickFunction TickFunction;

	struct TickPendingExecuteCue
	{
		AActor* Avatar = nullptr;
		float ExecuteTime = 0;
		float CurrentTime = 0;
		FGameplayTag CueTag;
		FGameplayCueParameters Params;
	};

	TArray<int32> PendingRemovals;
	TArray<TickPendingExecuteCue> PendingAddCues;
	TArray<TickPendingExecuteCue> PendingExecuteCues;
	bool bAdding = false;
#if WITH_EDITOR
	bool bAlreadyInitializedInSingleProcess = false;
#endif

public:
	UArcGameplayCueManager();

	virtual void Tick(float DeltaTime) override;

	void AddPendingExecuteCue(AActor* InAvatar
							  , float InExecuteTime
							  , const FGameplayTag& InCueTag
							  , const FGameplayCueParameters& InParams);

	virtual TStatId GetStatId() const override;

	virtual UWorld* GetTickableGameObjectWorld() const override;

private:
	void OnWorldPostInitialization(UWorld* InWorld
								   , const UWorld::InitializationValues InitValues);

	void OnWorldPreDestroy(UWorld* InWorld);

public:
	static UArcGameplayCueManager* Get();

	//~UGameplayCueManager interface
	virtual void OnCreated() override;

	virtual bool ShouldAsyncLoadRuntimeObjectLibraries() const override;

	virtual bool ShouldSyncLoadMissingGameplayCues() const override;

	virtual bool ShouldAsyncLoadMissingGameplayCues() const override;

	//~End of UGameplayCueManager interface

	static void DumpGameplayCues(const TArray<FString>& Args);

	// When delay loading cues, this will load the cues that must be always loaded anyway
	void LoadAlwaysLoadedCues();

	// Updates the bundles for the singular gameplay cue primary asset
	void RefreshGameplayCuePrimaryAsset();

private:
	void OnGameplayTagLoaded(const FGameplayTag& Tag);

	void HandlePostGarbageCollect();

	void ProcessLoadedTags();

	void ProcessTagToPreload(const FGameplayTag& Tag
							 , UObject* OwningObject);

	void OnPreloadCueComplete(FSoftObjectPath Path
							  , TWeakObjectPtr<UObject> OwningObject
							  , bool bAlwaysLoadedCue);

	void RegisterPreloadedCue(UClass* LoadedGameplayCueClass
							  , UObject* OwningObject);

	void HandlePostLoadMap(UWorld* NewWorld);

	void UpdateDelayLoadDelegateListeners();

	bool ShouldDelayLoadGameplayCues() const;

private:
	struct FLoadedGameplayTagToProcessData
	{
		FGameplayTag Tag;
		TWeakObjectPtr<UObject> WeakOwner;

		FLoadedGameplayTagToProcessData()
		{
		}

		FLoadedGameplayTagToProcessData(const FGameplayTag& InTag
										, const TWeakObjectPtr<UObject>& InWeakOwner)
			: Tag(InTag)
			, WeakOwner(InWeakOwner)
		{
		}
	};

private:
	// Cues that were preloaded on the client due to being referenced by content
	UPROPERTY(transient)
	TSet<TObjectPtr<UClass>> PreloadedCues;
	TMap<FObjectKey, TSet<FObjectKey>> PreloadedCueReferencers;

	// Cues that were preloaded on the client and will always be loaded (code referenced
	// or explicitly always loaded)
	UPROPERTY(transient)
	TSet<TObjectPtr<UClass>> AlwaysLoadedCues;

	TArray<FLoadedGameplayTagToProcessData> LoadedGameplayTagsToProcess;
	FCriticalSection LoadedGameplayTagsToProcessCS;
	bool bProcessLoadedTagsAfterGC = false;
};
