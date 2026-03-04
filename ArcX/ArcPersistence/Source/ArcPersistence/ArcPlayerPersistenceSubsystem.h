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

struct FArcPlayerProviderDescriptor;

/**
 * Manages per-player persistent data.
 *
 * Supports two modes:
 * 1. Descriptor-based (automatic): Provider descriptors registered at module load
 *    map component classes to domains. The subsystem hooks PostLogin/Logout to track
 *    players and resolves components at save/load time via the provider tree.
 *
 * 2. Direct (explicit): SaveObject/LoadObject for callers needing full control
 *    over the domain string (e.g., debugger saving under "characters/{name}/...").
 *
 * GameInstanceSubsystem — persists across map transitions.
 */
UCLASS()
class ARCPERSISTENCE_API UArcPlayerPersistenceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Player Tracking ────────────────────────────────────────────────

	/** Manual player registration for editor/debugger (no real login flow). */
	void RegisterPlayer(const FGuid& PlayerId, APlayerController* PC);
	void UnregisterPlayer(const FGuid& PlayerId);

	// ── Descriptor-Based API (automatic) ───────────────────────────────

	/** Save all resolved provider components for a player. */
	void SavePlayerData(const FGuid& PlayerId);

	/** Load all data for a player into cache, apply to resolved providers. */
	void LoadPlayerData(const FGuid& PlayerId);

	/** Save data for all tracked players. */
	void SaveAllPlayerData();

	// ── Direct API (explicit domain + object) ──────────────────────────

	/** Save a specific object under an explicit domain. */
	void SaveObject(const FGuid& PlayerId, const FString& Domain, UObject* Source);

	/** Load data for an explicit domain and apply to target object. */
	void LoadObject(const FGuid& PlayerId, const FString& Domain, UObject* Target);

	// ── Query + Management ─────────────────────────────────────────────

	/** List all stored domain strings for a player from the backend. */
	TArray<FString> ListPlayerDomains(const FGuid& PlayerId);

	/** Delete a single domain from backend and cache. */
	void DeletePlayerDomain(const FGuid& PlayerId, const FString& Domain);

	/** Clear in-memory cache for a player. */
	void ClearPlayerCache(const FGuid& PlayerId);

	/** Check if data has been loaded into cache for a player. */
	bool IsPlayerDataLoaded(const FGuid& PlayerId) const;

	/** Get cached raw bytes for a specific domain. Returns nullptr if not cached. */
	const TArray<uint8>* GetCachedData(const FGuid& PlayerId, const FString& Domain) const;

	// ── Legacy Instance-Bound API ──────────────────────────────────────

	void RegisterPlayerDataProvider(const FGuid& PlayerId, const FString& Domain, UObject* Source);
	void UnregisterPlayerDataProvider(const FGuid& PlayerId, const FString& Domain);
	void ApplyPlayerData(const FGuid& PlayerId, const FString& Domain);

private:
	// ── Legacy ─────────────────────────────────────────────────────────

	struct FPlayerDataProvider
	{
		FString Domain;
		TWeakObjectPtr<UObject> Source;
	};

	TMap<FGuid, TMap<FString, FPlayerDataProvider>> PlayerProviders;

	// ── Player tracking ────────────────────────────────────────────────

	/** PlayerId -> PlayerController (root of provider tree). */
	TMap<FGuid, TWeakObjectPtr<APlayerController>> TrackedPlayers;

	/** Cached save data: PlayerId -> (Domain -> bytes). */
	TMap<FGuid, TMap<FString, TArray<uint8>>> CachedPlayerData;

	// ── Internal helpers ───────────────────────────────────────────────

	class IArcPersistenceBackend* GetBackend() const;

	/** Resolved leaf provider: object + full domain path. */
	struct FResolvedProvider
	{
		UObject* Object = nullptr;
		FString DomainPath;
	};

	/** Walk the provider tree from root (PC) to all serializable leaves. */
	TArray<FResolvedProvider> ResolveAllProviders(const FGuid& PlayerId) const;

	/** Apply serialized data to a UObject using the serializer registry. */
	void ApplyDataToObject(UObject* Target, const TArray<uint8>& Data, const FString& Domain);

	/** Serialize a UObject and persist via backend. */
	void SaveObjectInternal(const FGuid& PlayerId, const FString& Domain, UObject* Source);

	/** Extract domain from a full key by stripping the player prefix. */
	static FString ExtractDomain(const FString& Key, const FString& PlayerPrefix);

	// ── PostLogin / Logout hooks ───────────────────────────────────────

	void OnPlayerPostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);
	void OnPlayerLogout(AGameModeBase* GameMode, AController* Exiting);

	// ── Legacy internal ────────────────────────────────────────────────

	void ApplyDataToProvider(const FPlayerDataProvider& Provider, const TArray<uint8>& Data);
	void SaveProvider(const FGuid& PlayerId, const FPlayerDataProvider& Provider);
};
