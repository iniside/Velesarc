/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2026 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
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

#include "ArcWorldPersistenceSubsystem.h"

#include "ArcPersistenceSubsystem.h"
#include "ArcPersistenceEvents.h"
#include "Engine/GameInstance.h"
#include "Serialization/ArcSerializerRegistry.h"
#include "Serialization/ArcJsonSaveArchive.h"
#include "Serialization/ArcJsonLoadArchive.h"
#include "Storage/ArcPersistenceBackend.h"

#include "ArcPersistenceClassRegistry.h"
#include "ArcPersistentIdComponent.h"
#include "Async/Async.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Streaming/LevelStreamingDelegates.h"
#include "Storage/ArcPersistenceKeyConvention.h"

void UArcWorldPersistenceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency<UArcPersistenceSubsystem>();

	// World Partition streaming support (like engine's LevelStreamingPersistenceManager)
	FLevelStreamingDelegates::OnLevelBeginMakingInvisible.AddUObject(
		this, &UArcWorldPersistenceSubsystem::HandleLevelBeginMakingInvisible);
	FLevelStreamingDelegates::OnLevelBeginMakingVisible.AddUObject(
		this, &UArcWorldPersistenceSubsystem::HandleLevelBeginMakingVisible);
}

void UArcWorldPersistenceSubsystem::Deinitialize()
{
	FLevelStreamingDelegates::OnLevelBeginMakingInvisible.RemoveAll(this);
	FLevelStreamingDelegates::OnLevelBeginMakingVisible.RemoveAll(this);

	Super::Deinitialize();
}

// -----------------------------------------------------------------------------
// Storage key helper
// -----------------------------------------------------------------------------

FString UArcWorldPersistenceSubsystem::MakeStorageKey(const FString& Key) const
{
	return UE::ArcPersistence::MakeWorldKey(CurrentWorldId.ToString(), Key);
}

// -----------------------------------------------------------------------------
// Core API
// -----------------------------------------------------------------------------

void UArcWorldPersistenceSubsystem::LoadWorldData(const FGuid& WorldId)
{
	LoadWorldDataAsync(WorldId).Get();
}

TFuture<void> UArcWorldPersistenceSubsystem::LoadWorldDataAsync(const FGuid& WorldId)
{
	auto Promise = MakeShared<TPromise<void>>();
	TFuture<void> Future = Promise->GetFuture();

	UArcPersistenceSubsystem* PersistenceSub = GetGameInstance()->GetSubsystem<UArcPersistenceSubsystem>();
	IArcPersistenceBackend* Backend = PersistenceSub->GetBackend();
	if (!Backend)
	{
		UE_LOG(LogTemp, Warning, TEXT("ArcWorldPersistence: No backend available for LoadWorldData"));
		Promise->SetValue();
		return Future;
	}

	ClearAll();
	CurrentWorldId = WorldId;

	// Broadcast started event on game thread
	{
		FArcPersistenceEvent Event;
		Event.Operation = EArcPersistenceOperation::Load;
		Event.Scope = EArcPersistenceScope::World;
		Event.ContextId = WorldId;
		PersistenceSub->OnPersistenceStarted.Broadcast(Event);
	}

	const FString Prefix = FString::Printf(TEXT("world/%s"), *WorldId.ToString());
	TFuture<FArcPersistenceListResult> ListFuture = Backend->ListEntries(Prefix);

	// Move to background to wait for list result and load entries
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, Promise, Backend, PersistenceSub, WorldId, Prefix, ListFuture = MoveTemp(ListFuture)]() mutable
	{
		FArcPersistenceListResult ListResult = ListFuture.Get();

		if (!ListResult.bSuccess)
		{
			UE_LOG(LogTemp, Warning, TEXT("ArcWorldPersistence: ListEntries failed: %s"), *ListResult.Error);
			AsyncTask(ENamedThreads::GameThread, [Promise]()
			{
				Promise->SetValue();
			});
			return;
		}

		// Load each entry serially in background
		const FString PrefixWithSlash = Prefix / TEXT("");
		TArray<TPair<FString, TArray<uint8>>> LoadedEntries;

		for (const FString& StorageKey : ListResult.Keys)
		{
			FString OriginalKey = StorageKey;
			if (OriginalKey.StartsWith(PrefixWithSlash))
			{
				OriginalKey.RightChopInline(PrefixWithSlash.Len());
			}

			FArcPersistenceLoadResult LoadResult = Backend->LoadEntry(StorageKey).Get();
			if (!LoadResult.bSuccess)
			{
				continue;
			}

			LoadedEntries.Emplace(MoveTemp(OriginalKey), MoveTemp(LoadResult.Data));
		}

		// Back to game thread: populate caches and broadcast completed
		AsyncTask(ENamedThreads::GameThread, [this, Promise, PersistenceSub, WorldId, LoadedEntries = MoveTemp(LoadedEntries)]() mutable
		{
			for (auto& Entry : LoadedEntries)
			{
				const FString& OriginalKey = Entry.Key;

				// Parse metadata for type name and tombstone state
				FArcJsonLoadArchive TempAr;
				if (TempAr.InitializeFromData(Entry.Value))
				{
					if (TempAr.BeginStruct(FName("_meta")))
					{
						FString TypeNameStr;
						if (TempAr.ReadProperty(FName("type"), TypeNameStr))
						{
							CachedTypeNames.Add(OriginalKey, FName(*TypeNameStr));
						}

						bool bTombstoned = false;
						if (TempAr.ReadProperty(FName("tombstoned"), bTombstoned) && bTombstoned)
						{
							TombstonedKeys.Add(OriginalKey);
						}
						TempAr.EndStruct();
					}
				}

				CachedData.Add(OriginalKey, MoveTemp(Entry.Value));
			}

			UE_LOG(LogTemp, Log, TEXT("ArcWorldPersistence: Loaded %d entries for world %s (%d tombstoned)"),
				CachedData.Num(), *WorldId.ToString(), TombstonedKeys.Num());

			// Broadcast completed event
			{
				FArcPersistenceEvent Event;
				Event.Operation = EArcPersistenceOperation::Load;
				Event.Scope = EArcPersistenceScope::World;
				Event.ContextId = WorldId;
				PersistenceSub->OnPersistenceCompleted.Broadcast(Event);
			}

			Promise->SetValue();
		});
	});

	return Future;
}

void UArcWorldPersistenceSubsystem::SaveWorldData(const FGuid& WorldId)
{
	SaveWorldDataAsync(WorldId).Get();
}

TFuture<void> UArcWorldPersistenceSubsystem::SaveWorldDataAsync(const FGuid& WorldId)
{
	auto Promise = MakeShared<TPromise<void>>();
	TFuture<void> Future = Promise->GetFuture();

	UArcPersistenceSubsystem* PersistenceSub = GetGameInstance()->GetSubsystem<UArcPersistenceSubsystem>();
	IArcPersistenceBackend* Backend = PersistenceSub->GetBackend();
	if (!Backend)
	{
		UE_LOG(LogTemp, Warning, TEXT("ArcWorldPersistence: No backend available for SaveWorldData"));
		Promise->SetValue();
		return Future;
	}

	// Broadcast started event on game thread
	{
		FArcPersistenceEvent Event;
		Event.Operation = EArcPersistenceOperation::Save;
		Event.Scope = EArcPersistenceScope::World;
		Event.ContextId = WorldId;
		PersistenceSub->OnPersistenceStarted.Broadcast(Event);
	}

	// Game thread: discover all persistent actors in the world and serialize.
	TArray<TPair<FString, TArray<uint8>>> Entries;

	UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	if (World)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			UArcPersistentIdComponent* IdComp = (*It)->FindComponentByClass<UArcPersistentIdComponent>();
			if (!IdComp || !IdComp->PersistenceId.IsValid())
			{
				continue;
			}

			if (!ArcPersistence::IsClassPersistent((*It)->GetClass()))
			{
				continue;
			}

			const FString Key = IdComp->PersistenceId.ToString();
			TArray<uint8> Data = SerializeObject(*It);
			Entries.Emplace(MakeStorageKey(Key), MoveTemp(Data));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("ArcWorldPersistence: Saving %d objects for world %s"),
		Entries.Num(), *WorldId.ToString());

	// Submit to backend asynchronously
	TFuture<FArcPersistenceResult> SaveFuture = Backend->SaveEntries(MoveTemp(Entries));

	// Wait in background, then broadcast completed on game thread
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Promise, PersistenceSub, WorldId, SaveFuture = MoveTemp(SaveFuture)]() mutable
	{
		SaveFuture.Get();

		AsyncTask(ENamedThreads::GameThread, [Promise, PersistenceSub, WorldId]()
		{
			FArcPersistenceEvent Event;
			Event.Operation = EArcPersistenceOperation::Save;
			Event.Scope = EArcPersistenceScope::World;
			Event.ContextId = WorldId;
			PersistenceSub->OnPersistenceCompleted.Broadcast(Event);

			Promise->SetValue();
		});
	});

	return Future;
}

// -----------------------------------------------------------------------------
// Actor Registration
// -----------------------------------------------------------------------------

void UArcWorldPersistenceSubsystem::RegisterActor(AActor* Actor, const FGuid& Id)
{
	check(Actor);
	check(Id.IsValid());

	RegisterActor(Actor, Id.ToString());
}

void UArcWorldPersistenceSubsystem::RegisterActor(AActor* Actor, const FString& Key)
{
	check(Actor);
	check(!Key.IsEmpty());

	if (!ArcPersistence::IsClassPersistent(Actor->GetClass()))
	{
#if WITH_EDITOR
		UE_LOG(LogTemp, Warning,
			TEXT("ArcWorldPersistence: Class %s is not whitelisted for persistence. Skipping registration with key '%s'."),
			*Actor->GetClass()->GetName(), *Key);
#endif
		return;
	}

	// Apply cached data immediately if available
	if (const TArray<uint8>* Data = CachedData.Find(Key))
	{
		if (!TombstonedKeys.Contains(Key))
		{
			ApplyDataToObject(*Data, Actor);
		}
	}
}

// -----------------------------------------------------------------------------
// Destruction Tracking
// -----------------------------------------------------------------------------

void UArcWorldPersistenceSubsystem::OnActorDestroyed(AActor* Actor, const FString& Key)
{
	if (Key.IsEmpty())
	{
		return;
	}

	const UClass* ActorClass = Actor->GetClass();
	const FArcPersistenceSerializerInfo* SerializerInfo = FArcSerializerRegistry::Get().Find(ActorClass);

	if (SerializerInfo && SerializerInfo->bSupportsTombstones)
	{
		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(SerializerInfo->Version);

		SaveAr.BeginStruct(FName("_meta"));
		SaveAr.WriteProperty(FName("type"), FString(ActorClass->GetFName().ToString()));
		SaveAr.WriteProperty(FName("tombstoned"), true);
		SaveAr.EndStruct();

		SerializerInfo->SaveFunc(Actor, SaveAr);

		TArray<uint8> Data = SaveAr.Finalize();

		UArcPersistenceSubsystem* PersistenceSub = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UArcPersistenceSubsystem>() : nullptr;
		IArcPersistenceBackend* Backend = PersistenceSub ? PersistenceSub->GetBackend() : nullptr;
		if (Backend)
		{
			Backend->SaveEntry(MakeStorageKey(Key), MoveTemp(Data));
		}

		TombstonedKeys.Add(Key);
	}
}

// -----------------------------------------------------------------------------
// Level Streaming Handlers (World Partition)
// -----------------------------------------------------------------------------

void UArcWorldPersistenceSubsystem::HandleLevelBeginMakingInvisible(
	UWorld* World, const ULevelStreaming* StreamingLevel, ULevel* LoadedLevel)
{
	if (!IsValid(LoadedLevel) || !CurrentWorldId.IsValid())
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI || World != GI->GetWorld())
	{
		return;
	}

	// Discover persistent actors in the unloading level, serialize and cache.
	TArray<TPair<FString, TArray<uint8>>> Entries;

	ForEachObjectWithOuter(LoadedLevel, [this, &Entries](UObject* Object)
	{
		AActor* Actor = Cast<AActor>(Object);
		if (!Actor)
		{
			return;
		}

		UArcPersistentIdComponent* IdComp = Actor->FindComponentByClass<UArcPersistentIdComponent>();
		if (!IdComp || !IdComp->PersistenceId.IsValid())
		{
			return;
		}

		if (!ArcPersistence::IsClassPersistent(Actor->GetClass()))
		{
			return;
		}

		const FString Key = IdComp->PersistenceId.ToString();
		TArray<uint8> Data = SerializeObject(Actor);
		CachedData.Add(Key, Data);
		Entries.Emplace(MakeStorageKey(Key), MoveTemp(Data));
	}, /* bIncludeNestedObjects */ false, RF_NoFlags, EInternalObjectFlags::Garbage);

	// Batch-save to backend asynchronously
	if (!Entries.IsEmpty())
	{
		UArcPersistenceSubsystem* PersistenceSub = GI->GetSubsystem<UArcPersistenceSubsystem>();
		IArcPersistenceBackend* Backend = PersistenceSub ? PersistenceSub->GetBackend() : nullptr;
		if (Backend)
		{
			Backend->SaveEntries(MoveTemp(Entries));
		}
	}
}

void UArcWorldPersistenceSubsystem::HandleLevelBeginMakingVisible(
	UWorld* World, const ULevelStreaming* StreamingLevel, ULevel* LoadedLevel)
{
	if (!IsValid(LoadedLevel) || !CurrentWorldId.IsValid())
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI || World != GI->GetWorld())
	{
		return;
	}

	// Collect persistent actors in this level — split into cached (apply now)
	// and cache-miss (batch-load from backend, then apply).
	TArray<TPair<FString, TWeakObjectPtr<AActor>>> KeysToLoad;

	ForEachObjectWithOuter(LoadedLevel, [this, &KeysToLoad](UObject* Object)
	{
		AActor* Actor = Cast<AActor>(Object);
		if (!Actor)
		{
			return;
		}

		UArcPersistentIdComponent* IdComp = Actor->FindComponentByClass<UArcPersistentIdComponent>();
		if (!IdComp || !IdComp->PersistenceId.IsValid())
		{
			return;
		}

		const FString Key = IdComp->PersistenceId.ToString();

		if (!ArcPersistence::IsClassPersistent(Actor->GetClass()))
		{
			return;
		}

		if (const TArray<uint8>* Data = CachedData.Find(Key))
		{
			// Cache hit — apply immediately
			if (!TombstonedKeys.Contains(Key))
			{
				ApplyDataToObject(*Data, Actor);
			}
		}
		else if (!PendingLoadKeys.Contains(Key))
		{
			// Cache miss — queue for batch load
			KeysToLoad.Emplace(Key, Actor);
		}
	}, /* bIncludeNestedObjects */ false, RF_NoFlags, EInternalObjectFlags::Garbage);

	if (KeysToLoad.IsEmpty())
	{
		return;
	}

	// Batch-load all cache-miss keys from backend in one background task
	UArcPersistenceSubsystem* PersistenceSub = GI->GetSubsystem<UArcPersistenceSubsystem>();
	IArcPersistenceBackend* Backend = PersistenceSub ? PersistenceSub->GetBackend() : nullptr;
	if (!Backend)
	{
		return;
	}

	// Launch all backend loads, collect futures
	TArray<TPair<FString, TFuture<FArcPersistenceLoadResult>>> LoadFutures;
	LoadFutures.Reserve(KeysToLoad.Num());

	for (auto& Pair : KeysToLoad)
	{
		PendingLoadKeys.Add(Pair.Key);
		LoadFutures.Emplace(Pair.Key, Backend->LoadEntry(MakeStorageKey(Pair.Key)));
	}

	TWeakObjectPtr<UArcWorldPersistenceSubsystem> WeakThis = this;

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
		[WeakThis, KeysToLoad = MoveTemp(KeysToLoad), LoadFutures = MoveTemp(LoadFutures)]() mutable
	{
		// Wait for all loads in background
		TArray<TPair<FString, FArcPersistenceLoadResult>> Results;
		Results.Reserve(LoadFutures.Num());

		for (auto& Pair : LoadFutures)
		{
			Results.Emplace(Pair.Key, Pair.Value.Get());
		}

		// Apply all results on game thread
		AsyncTask(ENamedThreads::GameThread,
			[WeakThis, KeysToLoad = MoveTemp(KeysToLoad), Results = MoveTemp(Results)]() mutable
		{
			UArcWorldPersistenceSubsystem* Self = WeakThis.Get();
			if (!Self)
			{
				return;
			}

			for (int32 i = 0; i < Results.Num(); ++i)
			{
				const FString& Key = Results[i].Key;
				FArcPersistenceLoadResult& LoadResult = Results[i].Value;

				Self->PendingLoadKeys.Remove(Key);

				if (!LoadResult.bSuccess || LoadResult.Data.IsEmpty())
				{
					continue;
				}

				// Parse metadata
				FArcJsonLoadArchive TempAr;
				if (TempAr.InitializeFromData(LoadResult.Data))
				{
					if (TempAr.BeginStruct(FName("_meta")))
					{
						FString TypeNameStr;
						if (TempAr.ReadProperty(FName("type"), TypeNameStr))
						{
							Self->CachedTypeNames.Add(Key, FName(*TypeNameStr));
						}

						bool bTombstoned = false;
						if (TempAr.ReadProperty(FName("tombstoned"), bTombstoned) && bTombstoned)
						{
							Self->TombstonedKeys.Add(Key);
						}
						TempAr.EndStruct();
					}
				}

				Self->CachedData.Add(Key, MoveTemp(LoadResult.Data));

				// Apply to actor if still alive and not tombstoned
				TWeakObjectPtr<AActor> WeakActor = KeysToLoad[i].Value;
				if (WeakActor.IsValid() && !Self->TombstonedKeys.Contains(Key))
				{
					if (const TArray<uint8>* CachedPtr = Self->CachedData.Find(Key))
					{
						Self->ApplyDataToObject(*CachedPtr, WeakActor.Get());
					}
				}
			}
		});
	});
}

// -----------------------------------------------------------------------------
// Spawn Support
// -----------------------------------------------------------------------------

TArray<FArcPendingSpawn> UArcWorldPersistenceSubsystem::GetPendingSpawns() const
{
	// Build a set of persistence keys that already have live actors.
	TSet<FString> LiveKeys;

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (World)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			UArcPersistentIdComponent* IdComp = (*It)->FindComponentByClass<UArcPersistentIdComponent>();
			if (IdComp && IdComp->PersistenceId.IsValid())
			{
				LiveKeys.Add(IdComp->PersistenceId.ToString());
			}
		}
	}

	TArray<FArcPendingSpawn> Result;

	for (const auto& Pair : CachedData)
	{
		const FString& Key = Pair.Key;

		if (TombstonedKeys.Contains(Key))
		{
			continue;
		}

		if (LiveKeys.Contains(Key))
		{
			continue;
		}

		FArcPendingSpawn Spawn;
		Spawn.Key = Key;
		if (const FName* TypeName = CachedTypeNames.Find(Key))
		{
			Spawn.TypeName = *TypeName;
		}
		Result.Add(Spawn);
	}

	return Result;
}

bool UArcWorldPersistenceSubsystem::IsTombstoned(const FString& Key) const
{
	return TombstonedKeys.Contains(Key);
}

// -----------------------------------------------------------------------------
// Internal: Serialization
// -----------------------------------------------------------------------------

TArray<uint8> UArcWorldPersistenceSubsystem::SerializeObject(UObject* Object)
{
	const UStruct* Type = Object->GetClass();
	const FArcPersistenceSerializerInfo* Info = FArcSerializerRegistry::Get().FindOrDefault(Type);

	FArcJsonSaveArchive SaveAr;

	SaveAr.BeginStruct(FName("_meta"));
	SaveAr.WriteProperty(FName("type"), FString(Type->GetFName().ToString()));
	SaveAr.EndStruct();

	if (Info->PreSaveFunc)
	{
		UWorld* World = nullptr;
		if (AActor* Actor = Cast<AActor>(Object))
		{
			World = Actor->GetWorld();
		}
		if (World)
		{
			Info->PreSaveFunc(Object, *World);
		}
	}

	SaveAr.SetVersion(Info->Version);
	Info->SaveFunc(Object, SaveAr);

	return SaveAr.Finalize();
}

void UArcWorldPersistenceSubsystem::ApplyDataToObject(const TArray<uint8>& Data, UObject* Object)
{
	const UStruct* Type = Object->GetClass();
	const FArcPersistenceSerializerInfo* Info = FArcSerializerRegistry::Get().FindOrDefault(Type);

	FArcJsonLoadArchive LoadAr;
	if (!LoadAr.InitializeFromData(Data))
	{
		UE_LOG(LogTemp, Warning, TEXT("ArcWorldPersistence: Failed to parse cached data"));
		return;
	}

	if (LoadAr.GetVersion() != Info->Version)
	{
		UE_LOG(LogTemp, Log, TEXT("ArcWorldPersistence: Version mismatch (saved: %u, current: %u) — discarding"),
			LoadAr.GetVersion(), Info->Version);
		return;
	}

	Info->LoadFunc(Object, LoadAr);

	// Call PostLoad hook after data is applied
	if (Info->PostLoadFunc)
	{
		UWorld* World = nullptr;
		if (AActor* Actor = Cast<AActor>(Object))
		{
			World = Actor->GetWorld();
		}
		if (World)
		{
			Info->PostLoadFunc(Object, *World);
		}
	}
}

void UArcWorldPersistenceSubsystem::ClearAll()
{
	CachedData.Empty();
	CachedTypeNames.Empty();
	TombstonedKeys.Empty();
	PendingLoadKeys.Empty();
	CurrentWorldId.Invalidate();
}
