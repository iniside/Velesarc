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

#include "Subsystems/GameInstanceSubsystem.h"

#include "ArcPlayerPersistenceSubsystem.generated.h"

/**
 * Manages per-player data that survives map transitions and server hops.
 *
 * Uses a "data provider" pattern: game code registers UObjects as the source
 * for named domains (e.g., "inventory", "attributes"). The subsystem handles
 * serialization/deserialization via the serializer registry or reflection fallback.
 *
 * Data is loaded into a memory cache per player. When a provider is registered,
 * cached data is applied automatically.
 *
 * GameInstanceSubsystem so data persists across map transitions.
 */
UCLASS()
class ARCPERSISTENCE_API UArcPlayerPersistenceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Core API ────────────────────────────────────────────────────────

	/** Load all data for a player into memory cache. */
	void LoadPlayerData(const FGuid& PlayerId);

	/** Save all registered providers for a player. */
	void SavePlayerData(const FGuid& PlayerId);

	/** Save data for all connected players. */
	void SaveAllPlayerData();

	// ── Data Provider Registration ──────────────────────────────────────

	/**
	 * Register an object as the data source for a specific domain.
	 * If cached data already exists for this player+domain, applies it immediately.
	 *
	 * @param PlayerId  The player's unique identifier
	 * @param Domain    A plain string: "inventory", "attributes", "quickbar", etc.
	 * @param Source    The UObject whose SaveGame properties will be serialized
	 */
	void RegisterPlayerDataProvider(const FGuid& PlayerId, const FString& Domain, UObject* Source);

	void UnregisterPlayerDataProvider(const FGuid& PlayerId, const FString& Domain);

	/** Apply cached data to a registered provider. Called automatically on registration. */
	void ApplyPlayerData(const FGuid& PlayerId, const FString& Domain);

	/** Check if data has been loaded into cache for a player. */
	bool IsPlayerDataLoaded(const FGuid& PlayerId) const;

private:
	struct FPlayerDataProvider
	{
		FString Domain;
		TWeakObjectPtr<UObject> Source;
	};

	/** PlayerId -> (Domain -> Provider) */
	TMap<FGuid, TMap<FString, FPlayerDataProvider>> PlayerProviders;

	/** Cached save data: PlayerId -> (Domain -> bytes) */
	TMap<FGuid, TMap<FString, TArray<uint8>>> CachedPlayerData;

	void ApplyDataToProvider(const FPlayerDataProvider& Provider, const TArray<uint8>& Data);
	void SaveProvider(const FGuid& PlayerId, const FPlayerDataProvider& Provider);
};
