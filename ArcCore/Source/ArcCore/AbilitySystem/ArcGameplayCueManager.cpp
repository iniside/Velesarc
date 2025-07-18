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

#include "ArcCore/AbilitySystem/ArcGameplayCueManager.h"

#include "AbilitySystemGlobals.h"
#include "Async/Async.h"
#include "Engine/AssetManager.h"
#include "GameplayCueSet.h"
#include "GameplayTagsManager.h"
#include "UObject/UObjectThreadContext.h"

DEFINE_LOG_CATEGORY(LogArcGameplayCue);

void FArcGCMTickFunction::ExecuteTick(float DeltaTime
									  , ELevelTick TickType
									  , ENamedThreads::Type CurrentThread
									  , const FGraphEventRef& MyCompletionGraphEvent)
{
	CueManager->Tick(DeltaTime);
}

UArcGameplayCueManager::UArcGameplayCueManager()
{
	TickFunction.TickGroup = ETickingGroup::TG_PostPhysics;
	TickFunction.bRunOnAnyThread = false;
	TickFunction.bAllowTickOnDedicatedServer = false;
}

void UArcGameplayCueManager::Tick(float DeltaTime)
{
	PendingExecuteCues.Append(PendingAddCues);
	PendingAddCues.Empty();
	// for(int32 Idx = 0; Idx < PendingExecuteCues.Num(); Idx++)
	for (int32 Idx = PendingExecuteCues.Num() - 1; Idx >= 0; Idx--)
	{
		PendingExecuteCues[Idx].CurrentTime += DeltaTime;
		if (PendingExecuteCues[Idx].CurrentTime >= PendingExecuteCues[Idx].ExecuteTime)
		{
			PendingRemovals.Add(Idx);
			HandleGameplayCue(PendingExecuteCues[Idx].Avatar
				, PendingExecuteCues[Idx].CueTag
				, EGameplayCueEvent::Type::Executed
				, PendingExecuteCues[Idx].Params);
			PendingExecuteCues.RemoveAt(Idx
				, 1
				, EAllowShrinking::No);
		}
	}
	PendingExecuteCues.Shrink();
}

void UArcGameplayCueManager::AddPendingExecuteCue(AActor* InAvatar
												  , float InExecuteTime
												  , const FGameplayTag& InCueTag
												  , const FGameplayCueParameters& InParams)
{
	TickPendingExecuteCue Pending{InAvatar, InExecuteTime, 0, InCueTag, InParams};
	PendingAddCues.Add(Pending);
	// PendingExecuteCues.Add(Pending);
}

TStatId UArcGameplayCueManager::GetStatId() const
{
	return GetStatID();
}

UWorld* UArcGameplayCueManager::GetTickableGameObjectWorld() const
{
	return GetWorld();
}

void UArcGameplayCueManager::OnWorldPostInitialization(UWorld* InWorld
													   , const UWorld::InitializationValues InitValues)
{
	PendingExecuteCues.Empty();
	PendingRemovals.Empty();
	if (InWorld->IsEditorWorld() && !InWorld->IsPlayInEditor())
	{
		return;
	}
#if WITH_EDITOR
	if (bAlreadyInitializedInSingleProcess == false)
	{
		bAlreadyInitializedInSingleProcess = true;
	}
	else
	{
		return;
	}
#endif
	if (InWorld->PersistentLevel)
	{
		TickFunction.CueManager = this;
		TickFunction.RegisterTickFunction(InWorld->PersistentLevel);
	}
}

void UArcGameplayCueManager::OnWorldPreDestroy(UWorld* InWorld)
{
	PendingExecuteCues.Empty();
	PendingRemovals.Empty();
	if (InWorld->IsEditorWorld() && !InWorld->IsPlayInEditor())
	{
		return;
	}
#if WITH_EDITOR
	bAlreadyInitializedInSingleProcess = false;
#endif

	TickFunction.UnRegisterTickFunction();
	TickFunction.CueManager = nullptr;
}

enum class ELyraEditorLoadMode
{
	// Loads all cues upfront; longer loading speed in the editor but short PIE times and
	// effects never fail to play
	LoadUpfront
	,

	// Outside of editor: Async loads as cue tag are registered
	// In editor: Async loads when cues are invoked
	//   Note: This can cause some 'why didn't I see the effect for X' issues in PIE and
	//   is good for iteration speed but otherwise bad for designers
	PreloadAsCuesAreReferenced_GameOnly
	,

	// Async loads as cue tag are registered
	PreloadAsCuesAreReferenced
};

namespace LyraGameplayCueManagerCvars
{
	static FAutoConsoleCommand CVarDumpGameplayCues(TEXT("Lyra.DumpGameplayCues")
		, TEXT("Shows all assets that were loaded via LyraGameplayCueManager and are currently in memory.")
		, FConsoleCommandWithArgsDelegate::CreateStatic(UArcGameplayCueManager::DumpGameplayCues));

	static ELyraEditorLoadMode LoadMode = ELyraEditorLoadMode::LoadUpfront;
} // namespace LyraGameplayCueManagerCvars

const bool bPreloadEvenInEditor = true;

//////////////////////////////////////////////////////////////////////

struct FGameplayCueTagThreadSynchronizeGraphTask : public FAsyncGraphTaskBase
{
	TFunction<void()> TheTask;

	FGameplayCueTagThreadSynchronizeGraphTask(TFunction<void()>&& Task)
		: TheTask(MoveTemp(Task))
	{
	}

	void DoTask(ENamedThreads::Type CurrentThread
				, const FGraphEventRef& MyCompletionGraphEvent)
	{
		TheTask();
	}

	ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::GameThread;
	}
};

//////////////////////////////////////////////////////////////////////

UArcGameplayCueManager* UArcGameplayCueManager::Get()
{
	return Cast<UArcGameplayCueManager>(UAbilitySystemGlobals::Get().GetGameplayCueManager());
}

void UArcGameplayCueManager::OnCreated()
{
	Super::OnCreated();

	UpdateDelayLoadDelegateListeners();
}

void UArcGameplayCueManager::LoadAlwaysLoadedCues()
{
	if (ShouldDelayLoadGameplayCues())
	{
		UGameplayTagsManager& TagManager = UGameplayTagsManager::Get();

		//@TODO: Try to collect these by filtering GameplayCue. tags out of native
		// gameplay tags?
		TArray<FName> AdditionalAlwaysLoadedCueTags;

		for (const FName& CueTagName : AdditionalAlwaysLoadedCueTags)
		{
			FGameplayTag CueTag = TagManager.RequestGameplayTag(CueTagName
				, /*ErrorIfNotFound=*/false);
			if (CueTag.IsValid())
			{
				ProcessTagToPreload(CueTag
					, nullptr);
			}
			else
			{
				UE_LOG(LogArcGameplayCue
					, Warning
					, TEXT( "UArcGameplayCueManager::AdditionalAlwaysLoadedCueTags contains invalid tag %s")
					, *CueTagName.ToString());
			}
		}
	}
}

bool UArcGameplayCueManager::ShouldAsyncLoadRuntimeObjectLibraries() const
{
	switch (LyraGameplayCueManagerCvars::LoadMode)
	{
		case ELyraEditorLoadMode::LoadUpfront:
			return true;
		case ELyraEditorLoadMode::PreloadAsCuesAreReferenced_GameOnly:
#if WITH_EDITOR
			if (GIsEditor)
			{
				return false;
			}
#endif
			break;
		case ELyraEditorLoadMode::PreloadAsCuesAreReferenced:
			break;
	}

	return !ShouldDelayLoadGameplayCues();
}

bool UArcGameplayCueManager::ShouldSyncLoadMissingGameplayCues() const
{
	return false;
}

bool UArcGameplayCueManager::ShouldAsyncLoadMissingGameplayCues() const
{
	return true;
}

void UArcGameplayCueManager::DumpGameplayCues(const TArray<FString>& Args)
{
	UArcGameplayCueManager* GCM = Cast<UArcGameplayCueManager>(UAbilitySystemGlobals::Get().GetGameplayCueManager());
	if (!GCM)
	{
		UE_LOG(LogArcGameplayCue
			, Error
			, TEXT("DumpGameplayCues failed. No UArcGameplayCueManager found."));
		return;
	}

	const bool bIncludeRefs = Args.Contains(TEXT("Refs"));

	UE_LOG(LogArcGameplayCue
		, Log
		, TEXT("=========== Dumping Always Loaded Gameplay Cue Notifies ==========="));
	for (UClass* CueClass : GCM->AlwaysLoadedCues)
	{
		UE_LOG(LogArcGameplayCue
			, Log
			, TEXT("  %s")
			, *GetPathNameSafe(CueClass));
	}

	UE_LOG(LogArcGameplayCue
		, Log
		, TEXT("=========== Dumping Preloaded Gameplay Cue Notifies ==========="));
	for (UClass* CueClass : GCM->PreloadedCues)
	{
		TSet<FObjectKey>* ReferencerSet = GCM->PreloadedCueReferencers.Find(CueClass);
		int32 NumRefs = ReferencerSet ? ReferencerSet->Num() : 0;
		UE_LOG(LogArcGameplayCue
			, Log
			, TEXT("  %s (%d refs)")
			, *GetPathNameSafe(CueClass)
			, NumRefs);
		if (bIncludeRefs && ReferencerSet)
		{
			for (const FObjectKey& Ref : *ReferencerSet)
			{
				UObject* RefObject = Ref.ResolveObjectPtr();
				UE_LOG(LogArcGameplayCue
					, Log
					, TEXT("    ^- %s")
					, *GetPathNameSafe(RefObject));
			}
		}
	}

	UE_LOG(LogArcGameplayCue
		, Log
		, TEXT("=========== Dumping Gameplay Cue Notifies loaded on demand ==========="));
	int32 NumMissingCuesLoaded = 0;
	if (GCM->RuntimeGameplayCueObjectLibrary.CueSet)
	{
		for (const FGameplayCueNotifyData& CueData : GCM->RuntimeGameplayCueObjectLibrary.CueSet->GameplayCueData)
		{
			if (CueData.LoadedGameplayCueClass && !GCM->AlwaysLoadedCues.Contains(CueData.LoadedGameplayCueClass) && !
				GCM->PreloadedCues.Contains(CueData.LoadedGameplayCueClass))
			{
				NumMissingCuesLoaded++;
				UE_LOG(LogArcGameplayCue
					, Log
					, TEXT("  %s")
					, *CueData.LoadedGameplayCueClass->GetPathName());
			}
		}
	}

	UE_LOG(LogArcGameplayCue
		, Log
		, TEXT("=========== Gameplay Cue Notify summary ==========="));
	UE_LOG(LogArcGameplayCue
		, Log
		, TEXT("  ... %d cues in always loaded list")
		, GCM->AlwaysLoadedCues.Num());
	UE_LOG(LogArcGameplayCue
		, Log
		, TEXT("  ... %d cues in preloaded list")
		, GCM->PreloadedCues.Num());
	UE_LOG(LogArcGameplayCue
		, Log
		, TEXT("  ... %d cues loaded on demand")
		, NumMissingCuesLoaded);
	UE_LOG(LogArcGameplayCue
		, Log
		, TEXT("  ... %d cues in total")
		, GCM->AlwaysLoadedCues.Num() + GCM->PreloadedCues.Num() + NumMissingCuesLoaded);
}

void UArcGameplayCueManager::OnGameplayTagLoaded(const FGameplayTag& Tag)
{
	FScopeLock ScopeLock(&LoadedGameplayTagsToProcessCS);
	bool bStartTask = LoadedGameplayTagsToProcess.Num() == 0;
	FUObjectSerializeContext* LoadContext = FUObjectThreadContext::Get().GetSerializeContext();
	UObject* OwningObject = LoadContext ? LoadContext->SerializedObject : nullptr;
	LoadedGameplayTagsToProcess.Emplace(Tag
		, OwningObject);
	if (bStartTask)
	{
		TGraphTask<FGameplayCueTagThreadSynchronizeGraphTask>::CreateTask().ConstructAndDispatchWhenReady([]()
		{
			if (GIsRunning)
			{
				if (UArcGameplayCueManager* StrongThis = Get())
				{
					// If we are garbage collecting we cannot call StaticFindObject
					// (or a few other static uobject functions), so we'll just wait
					// until the GC is over and process the tags then
					if (IsGarbageCollecting())
					{
						StrongThis->bProcessLoadedTagsAfterGC = true;
					}
					else
					{
						StrongThis->ProcessLoadedTags();
					}
				}
			}
		});
	}
}

void UArcGameplayCueManager::HandlePostGarbageCollect()
{
	if (bProcessLoadedTagsAfterGC)
	{
		ProcessLoadedTags();
	}
	bProcessLoadedTagsAfterGC = false;
}

void UArcGameplayCueManager::ProcessLoadedTags()
{
	TArray<FLoadedGameplayTagToProcessData> TaskLoadedGameplayTagsToProcess;
	{
		// Lock LoadedGameplayTagsToProcess just long enough to make a copy and clear
		FScopeLock TaskScopeLock(&LoadedGameplayTagsToProcessCS);
		TaskLoadedGameplayTagsToProcess = LoadedGameplayTagsToProcess;
		LoadedGameplayTagsToProcess.Empty();
	}

	// This might return during shutdown, and we don't want to proceed if that is the case
	if (GIsRunning)
	{
		if (RuntimeGameplayCueObjectLibrary.CueSet)
		{
			for (const FLoadedGameplayTagToProcessData& LoadedTagData : TaskLoadedGameplayTagsToProcess)
			{
				if (RuntimeGameplayCueObjectLibrary.CueSet->GameplayCueDataMap.Contains(LoadedTagData.Tag))
				{
					if (!LoadedTagData.WeakOwner.IsStale())
					{
						ProcessTagToPreload(LoadedTagData.Tag
							, LoadedTagData.WeakOwner.Get());
					}
				}
			}
		}
		else
		{
			UE_LOG(LogArcGameplayCue
				, Warning
				, TEXT( "UArcGameplayCueManager::OnGameplayTagLoaded processed loaded tag(s) but "
					"RuntimeGameplayCueObjectLibrary.CueSet was null. Skipping processing."));
		}
	}
}

void UArcGameplayCueManager::ProcessTagToPreload(const FGameplayTag& Tag
												 , UObject* OwningObject)
{
	switch (LyraGameplayCueManagerCvars::LoadMode)
	{
		case ELyraEditorLoadMode::LoadUpfront:
			return;
		case ELyraEditorLoadMode::PreloadAsCuesAreReferenced_GameOnly:
#if WITH_EDITOR
			if (GIsEditor)
			{
				return;
			}
#endif
			break;
		case ELyraEditorLoadMode::PreloadAsCuesAreReferenced:
			break;
	}

	check(RuntimeGameplayCueObjectLibrary.CueSet);

	int32* DataIdx = RuntimeGameplayCueObjectLibrary.CueSet->GameplayCueDataMap.Find(Tag);
	if (DataIdx && RuntimeGameplayCueObjectLibrary.CueSet->GameplayCueData.IsValidIndex(*DataIdx))
	{
		const FGameplayCueNotifyData& CueData = RuntimeGameplayCueObjectLibrary.CueSet->GameplayCueData[*DataIdx];

		UClass* LoadedGameplayCueClass = FindObject<UClass>(nullptr
			, *CueData.GameplayCueNotifyObj.ToString());
		if (LoadedGameplayCueClass)
		{
			RegisterPreloadedCue(LoadedGameplayCueClass
				, OwningObject);
		}
		else
		{
			bool bAlwaysLoadedCue = OwningObject == nullptr;
			TWeakObjectPtr<UObject> WeakOwner = OwningObject;
			StreamableManager.RequestAsyncLoad(CueData.GameplayCueNotifyObj
				, FStreamableDelegate::CreateUObject(this
					, &ThisClass::OnPreloadCueComplete
					, CueData.GameplayCueNotifyObj
					, WeakOwner
					, bAlwaysLoadedCue)
				, FStreamableManager::DefaultAsyncLoadPriority
				, false
				, false
				, TEXT("GameplayCueManager"));
		}
	}
}

void UArcGameplayCueManager::OnPreloadCueComplete(FSoftObjectPath Path
												  , TWeakObjectPtr<UObject> OwningObject
												  , bool bAlwaysLoadedCue)
{
	if (bAlwaysLoadedCue || OwningObject.IsValid())
	{
		if (UClass* LoadedGameplayCueClass = Cast<UClass>(Path.ResolveObject()))
		{
			RegisterPreloadedCue(LoadedGameplayCueClass
				, OwningObject.Get());
		}
	}
}

void UArcGameplayCueManager::RegisterPreloadedCue(UClass* LoadedGameplayCueClass
												  , UObject* OwningObject)
{
	check(LoadedGameplayCueClass);

	const bool bAlwaysLoadedCue = OwningObject == nullptr;
	if (bAlwaysLoadedCue)
	{
		AlwaysLoadedCues.Add(LoadedGameplayCueClass);
		PreloadedCues.Remove(LoadedGameplayCueClass);
		PreloadedCueReferencers.Remove(LoadedGameplayCueClass);
	}
	else if ((OwningObject != LoadedGameplayCueClass) && (OwningObject != LoadedGameplayCueClass->GetDefaultObject()) &&
			 !AlwaysLoadedCues.Contains(LoadedGameplayCueClass))
	{
		PreloadedCues.Add(LoadedGameplayCueClass);
		TSet<FObjectKey>& ReferencerSet = PreloadedCueReferencers.FindOrAdd(LoadedGameplayCueClass);
		ReferencerSet.Add(OwningObject);
	}
}

void UArcGameplayCueManager::HandlePostLoadMap(UWorld* NewWorld)
{
	if (RuntimeGameplayCueObjectLibrary.CueSet)
	{
		for (UClass* CueClass : AlwaysLoadedCues)
		{
			RuntimeGameplayCueObjectLibrary.CueSet->RemoveLoadedClass(CueClass);
		}

		for (UClass* CueClass : PreloadedCues)
		{
			RuntimeGameplayCueObjectLibrary.CueSet->RemoveLoadedClass(CueClass);
		}
	}

	for (auto CueIt = PreloadedCues.CreateIterator(); CueIt; ++CueIt)
	{
		TSet<FObjectKey>& ReferencerSet = PreloadedCueReferencers.FindChecked(*CueIt);
		for (auto RefIt = ReferencerSet.CreateIterator(); RefIt; ++RefIt)
		{
			if (!RefIt->ResolveObjectPtr())
			{
				RefIt.RemoveCurrent();
			}
		}
		if (ReferencerSet.Num() == 0)
		{
			PreloadedCueReferencers.Remove(*CueIt);
			CueIt.RemoveCurrent();
		}
	}
}

void UArcGameplayCueManager::UpdateDelayLoadDelegateListeners()
{
	UGameplayTagsManager::Get().OnGameplayTagLoadedDelegate.RemoveAll(this);
	FCoreUObjectDelegates::GetPostGarbageCollect().RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	switch (LyraGameplayCueManagerCvars::LoadMode)
	{
		case ELyraEditorLoadMode::LoadUpfront:
			return;
		case ELyraEditorLoadMode::PreloadAsCuesAreReferenced_GameOnly:
#if WITH_EDITOR
			if (GIsEditor)
			{
				return;
			}
#endif
			break;
		case ELyraEditorLoadMode::PreloadAsCuesAreReferenced:
			break;
	}

	UGameplayTagsManager::Get().OnGameplayTagLoadedDelegate.AddUObject(this
		, &ThisClass::OnGameplayTagLoaded);
	FCoreUObjectDelegates::GetPostGarbageCollect().AddUObject(this
		, &ThisClass::HandlePostGarbageCollect);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this
		, &ThisClass::HandlePostLoadMap);
}

bool UArcGameplayCueManager::ShouldDelayLoadGameplayCues() const
{
	const bool bClientDelayLoadGameplayCues = true;
	return !IsRunningDedicatedServer() && bClientDelayLoadGameplayCues;
}

const FPrimaryAssetType UFortAssetManager_GameplayCueRefsType = TEXT("GameplayCueRefs");
const FName UFortAssetManager_GameplayCueRefsName = TEXT("GameplayCueReferences");
const FName UFortAssetManager_LoadStateClient = FName(TEXT("Client"));

void UArcGameplayCueManager::RefreshGameplayCuePrimaryAsset()
{
	TArray<FTopLevelAssetPath> TopCuePaths;
	UGameplayCueSet* RuntimeGameplayCueSet = GetRuntimeCueSet();
	TArray<FSoftObjectPath> CuePaths;
	if (RuntimeGameplayCueSet)
	{
		RuntimeGameplayCueSet->GetSoftObjectPaths(CuePaths);
	}

	for (const FSoftObjectPath& CuePath : CuePaths)
	{
		TopCuePaths.Add(FTopLevelAssetPath(CuePath.GetAssetPath()));
	}

	FAssetBundleData BundleData;
	BundleData.AddBundleAssets(UFortAssetManager_LoadStateClient
		, TopCuePaths);

	FPrimaryAssetId PrimaryAssetId = FPrimaryAssetId(UFortAssetManager_GameplayCueRefsType
		, UFortAssetManager_GameplayCueRefsName);
	UAssetManager::Get().AddDynamicAsset(PrimaryAssetId
		, FSoftObjectPath()
		, BundleData);
}
