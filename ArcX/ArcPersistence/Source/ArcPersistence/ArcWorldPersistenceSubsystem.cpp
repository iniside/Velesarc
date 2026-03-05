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
#include "Async/Async.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Storage/ArcPersistenceKeyConvention.h"

void UArcWorldPersistenceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency<UArcPersistenceSubsystem>();

	WorldInitHandle = FWorldDelegates::OnPreWorldInitialization.AddUObject(
		this, &UArcWorldPersistenceSubsystem::HandleWorldInit);
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UArcWorldPersistenceSubsystem::HandleWorldCleanup);
}

void UArcWorldPersistenceSubsystem::Deinitialize()
{
	FWorldDelegates::OnPreWorldInitialization.Remove(WorldInitHandle);
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);

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

	// Game thread: serialize all registered objects into key-data pairs
	TArray<TPair<FString, TArray<uint8>>> Entries;
	for (auto& Pair : RegisteredObjects)
	{
		if (Pair.Value.IsValid())
		{
			TArray<uint8> Data = SerializeObject(Pair.Value.Get());
			Entries.Emplace(MakeStorageKey(Pair.Key), MoveTemp(Data));
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

	RegisteredObjects.Add(Key, Actor);

	if (const TArray<uint8>* Data = CachedData.Find(Key))
	{
		if (!TombstonedKeys.Contains(Key))
		{
			ApplyDataToObject(*Data, Actor);
		}
	}
}

void UArcWorldPersistenceSubsystem::UnregisterActor(const FString& Key)
{
	RegisteredObjects.Remove(Key);
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

		IArcPersistenceBackend* Backend = GetGameInstance()->GetSubsystem<UArcPersistenceSubsystem>()->GetBackend();
		if (Backend)
		{
			Backend->SaveEntry(MakeStorageKey(Key), MoveTemp(Data));
		}

		TombstonedKeys.Add(Key);
	}

	RegisteredObjects.Remove(Key);
}

// -----------------------------------------------------------------------------
// Spawn Support
// -----------------------------------------------------------------------------

TArray<FArcPendingSpawn> UArcWorldPersistenceSubsystem::GetPendingSpawns() const
{
	TArray<FArcPendingSpawn> Result;

	for (const auto& Pair : CachedData)
	{
		const FString& Key = Pair.Key;

		if (TombstonedKeys.Contains(Key))
		{
			continue;
		}

		if (RegisteredObjects.Contains(Key))
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
	RegisteredObjects.Empty();
	CachedData.Empty();
	CachedTypeNames.Empty();
	TombstonedKeys.Empty();
	CurrentWorldId.Invalidate();
}

// -----------------------------------------------------------------------------
// Lifecycle hooks
// -----------------------------------------------------------------------------

void UArcWorldPersistenceSubsystem::HandleWorldInit(UWorld* World, const UWorld::InitializationValues IVS)
{
	if (!World || World->WorldType != EWorldType::Game)
	{
		return;
	}
}

void UArcWorldPersistenceSubsystem::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	if (!World || World->WorldType != EWorldType::Game)
	{
		return;
	}

	if (CurrentWorldId.IsValid())
	{
		SaveWorldData(CurrentWorldId);
	}

	ClearAll();
}
