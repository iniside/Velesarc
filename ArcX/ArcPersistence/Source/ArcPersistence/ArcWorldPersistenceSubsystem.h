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

#pragma once

#include "Engine/World.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Async/Future.h"

#include "ArcWorldPersistenceSubsystem.generated.h"

/**
 * Info about a saved entry that has no registered live object yet.
 * Game code uses GetPendingSpawns() to decide what to spawn.
 */
struct FArcPendingSpawn
{
	/** The storage key this entry was saved under. */
	FString Key;

	/** The type name from _meta (if available). */
	FName TypeName;
};

/**
 * Manages persistence of world actors (level-placed and runtime-spawned).
 *
 * All registrations use string keys. For GUID-identified actors, the caller
 * converts the GUID to a string. For level-placed actors, the caller uses
 * whatever convention makes sense (actor path, etc.).
 *
 * Data is loaded into an in-memory cache before/during world load.
 * Actors register by key and receive cached data automatically.
 * Supports tombstone tracking via specialized serializers.
 *
 * GameInstanceSubsystem so data can be loaded before world finishes loading.
 */
UCLASS()
class ARCPERSISTENCE_API UArcWorldPersistenceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Core API ────────────────────────────────────────────────────────

	/** Load all world data into memory cache. Call before/during world load. */
	void LoadWorldData(const FGuid& WorldId);

	/** Save all registered objects. Call on world cleanup or manual checkpoint. */
	void SaveWorldData(const FGuid& WorldId);

	/** Async variant of LoadWorldData. Returns a future that completes when done. */
	TFuture<void> LoadWorldDataAsync(const FGuid& WorldId);

	/** Async variant of SaveWorldData. Returns a future that completes when done. */
	TFuture<void> SaveWorldDataAsync(const FGuid& WorldId);

	// ── Actor Registration ──────────────────────────────────────────────

	/** Register an actor by key. If cached data exists, applies immediately. */
	void RegisterActor(AActor* Actor, const FString& Key);

	/** Convenience: register by GUID (converted to string). */
	void RegisterActor(AActor* Actor, const FGuid& Id);

	/** Remove a registered actor. */
	void UnregisterActor(const FString& Key);

	// ── Destruction Tracking ────────────────────────────────────────────

	/** Called when an actor is destroyed. If its serializer supports tombstones, records it. */
	void OnActorDestroyed(AActor* Actor, const FString& Key);

	// ── Spawn Support ───────────────────────────────────────────────────

	/** Keys in save data with no registered live object — game decides what to spawn. */
	TArray<FArcPendingSpawn> GetPendingSpawns() const;

	/** Check if a key has been tombstoned. */
	bool IsTombstoned(const FString& Key) const;

	/** Get the current world ID (set by LoadWorldData). */
	const FGuid& GetCurrentWorldId() const { return CurrentWorldId; }

private:
	FGuid CurrentWorldId;

	/** Live objects keyed by their persistence key. */
	TMap<FString, TWeakObjectPtr<UObject>> RegisteredObjects;

	/** Cached save data loaded from backend, keyed by persistence key. */
	TMap<FString, TArray<uint8>> CachedData;

	/** Type names from _meta, keyed by persistence key. */
	TMap<FString, FName> CachedTypeNames;

	/** Keys that are tombstoned. */
	TSet<FString> TombstonedKeys;

	TArray<uint8> SerializeObject(UObject* Object);
	void ApplyDataToObject(const TArray<uint8>& Data, UObject* Object);
	FString MakeStorageKey(const FString& Key) const;

	void ClearAll();

	// ── Lifecycle hooks ─────────────────────────────────────────────────

	void HandleWorldInit(UWorld* World, const UWorld::InitializationValues IVS);
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

	FDelegateHandle WorldInitHandle;
	FDelegateHandle WorldCleanupHandle;
};
