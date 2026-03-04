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

#include "ArcPlayerPersistenceSubsystem.h"

#include "ArcPersistenceSubsystem.h"
#include "ArcPlayerProviderDescriptor.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Serialization/ArcSerializerRegistry.h"
#include "Serialization/ArcJsonSaveArchive.h"
#include "Serialization/ArcJsonLoadArchive.h"
#include "Storage/ArcPersistenceBackend.h"
#include "Subsystems/SubsystemCollection.h"
#include "Storage/ArcPersistenceKeyConvention.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcPlayerPersistence, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UArcPlayerPersistenceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UArcPersistenceSubsystem>();

	FGameModeEvents::OnGameModePostLoginEvent().AddUObject(
		this, &UArcPlayerPersistenceSubsystem::OnPlayerPostLogin);
	FGameModeEvents::OnGameModeLogoutEvent().AddUObject(
		this, &UArcPlayerPersistenceSubsystem::OnPlayerLogout);
}

void UArcPlayerPersistenceSubsystem::Deinitialize()
{
	FGameModeEvents::OnGameModePostLoginEvent().RemoveAll(this);
	FGameModeEvents::OnGameModeLogoutEvent().RemoveAll(this);
	Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
// PostLogin / Logout Hooks
// ─────────────────────────────────────────────────────────────────────────────

void UArcPlayerPersistenceSubsystem::OnPlayerPostLogin(
	AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	if (!NewPlayer)
	{
		return;
	}

	// Derive FGuid from the player's unique net ID
	APlayerState* PS = NewPlayer->GetPlayerState<APlayerState>();
	if (!PS)
	{
		return;
	}

	const FUniqueNetIdRepl& NetId = PS->GetUniqueId();
	if (!NetId.IsValid())
	{
		UE_LOG(LogArcPlayerPersistence, Warning,
			TEXT("PostLogin: Player has no valid UniqueNetId, skipping auto-tracking"));
		return;
	}

	// Hash the UniqueNetId string representation to FGuid
	const FString NetIdStr = NetId.ToString();
	const FGuid PlayerId = FGuid(
		FCrc::StrCrc32(*NetIdStr),
		FCrc::StrCrc32(*NetIdStr) ^ 0x50455253, // "PERS"
		FCrc::StrCrc32(*FString::Printf(TEXT("%s_player"), *NetIdStr)),
		0x504C4159 // "PLAY"
	);

	TrackedPlayers.Add(PlayerId, NewPlayer);

	UE_LOG(LogArcPlayerPersistence, Log,
		TEXT("PostLogin: Tracking player %s (NetId: %s)"),
		*PlayerId.ToString(), *NetIdStr);
}

void UArcPlayerPersistenceSubsystem::OnPlayerLogout(
	AGameModeBase* GameMode, AController* Exiting)
{
	// Find and remove the tracked player
	FGuid PlayerIdToRemove;
	for (const auto& Pair : TrackedPlayers)
	{
		if (Pair.Value.Get() == Exiting)
		{
			PlayerIdToRemove = Pair.Key;
			break;
		}
	}

	if (PlayerIdToRemove.IsValid())
	{
		TrackedPlayers.Remove(PlayerIdToRemove);
		UE_LOG(LogArcPlayerPersistence, Log,
			TEXT("Logout: Untracked player %s"), *PlayerIdToRemove.ToString());
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Player Tracking (manual)
// ─────────────────────────────────────────────────────────────────────────────

void UArcPlayerPersistenceSubsystem::RegisterPlayer(
	const FGuid& PlayerId, APlayerController* PC)
{
	if (PC && PlayerId.IsValid())
	{
		TrackedPlayers.Add(PlayerId, PC);
	}
}

void UArcPlayerPersistenceSubsystem::UnregisterPlayer(const FGuid& PlayerId)
{
	TrackedPlayers.Remove(PlayerId);
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal Helpers
// ─────────────────────────────────────────────────────────────────────────────

IArcPersistenceBackend* UArcPlayerPersistenceSubsystem::GetBackend() const
{
	UArcPersistenceSubsystem* PersistenceSub =
		GetGameInstance()->GetSubsystem<UArcPersistenceSubsystem>();
	return PersistenceSub ? PersistenceSub->GetBackend() : nullptr;
}

FString UArcPlayerPersistenceSubsystem::ExtractDomain(
	const FString& Key, const FString& PlayerPrefix)
{
	if (Key.StartsWith(PlayerPrefix))
	{
		return Key.Mid(PlayerPrefix.Len());
	}
	return FString();
}

TArray<UArcPlayerPersistenceSubsystem::FResolvedProvider>
UArcPlayerPersistenceSubsystem::ResolveAllProviders(const FGuid& PlayerId) const
{
	TArray<FResolvedProvider> Result;

	const TWeakObjectPtr<APlayerController>* PCPtr = TrackedPlayers.Find(PlayerId);
	if (!PCPtr || !PCPtr->IsValid())
	{
		return Result;
	}

	APlayerController* RootPC = PCPtr->Get();
	const FArcPlayerProviderRegistry& Registry = FArcPlayerProviderRegistry::Get();
	TArray<const FArcPlayerProviderDescriptor*> Leaves = Registry.GetSerializableDescriptors();

	for (const FArcPlayerProviderDescriptor* Leaf : Leaves)
	{
		TArray<const FArcPlayerProviderDescriptor*> Chain = Registry.BuildChain(*Leaf);
		if (Chain.IsEmpty())
		{
			continue;
		}

		// Walk the chain from root, resolving at each level
		UObject* Current = RootPC;
		bool bResolved = true;

		for (const FArcPlayerProviderDescriptor* Desc : Chain)
		{
			Current = Desc->ResolveFunc(Current);
			if (!Current)
			{
				bResolved = false;
				break;
			}
		}

		if (bResolved && Current)
		{
			FResolvedProvider Resolved;
			Resolved.Object = Current;
			Resolved.DomainPath = Registry.BuildDomainPath(*Leaf);
			Result.Add(Resolved);
		}
	}

	return Result;
}

void UArcPlayerPersistenceSubsystem::ApplyDataToObject(
	UObject* Target, const TArray<uint8>& Data, const FString& Domain)
{
	if (!Target)
	{
		return;
	}

	const UStruct* Type = Target->GetClass();
	const FArcPersistenceSerializerInfo* Info =
		FArcSerializerRegistry::Get().FindOrDefault(Type);

	FArcJsonLoadArchive LoadAr;
	if (!LoadAr.InitializeFromData(Data))
	{
		UE_LOG(LogArcPlayerPersistence, Warning,
			TEXT("Failed to parse data for domain '%s'"), *Domain);
		return;
	}

	if (LoadAr.GetVersion() != Info->Version)
	{
		UE_LOG(LogArcPlayerPersistence, Log,
			TEXT("Version mismatch for domain '%s' (saved: %u, current: %u) — discarding"),
			*Domain, LoadAr.GetVersion(), Info->Version);
		return;
	}

	Info->LoadFunc(Target, LoadAr);
}

void UArcPlayerPersistenceSubsystem::SaveObjectInternal(
	const FGuid& PlayerId, const FString& Domain, UObject* Source)
{
	if (!Source)
	{
		return;
	}

	IArcPersistenceBackend* Backend = GetBackend();
	if (!Backend)
	{
		return;
	}

	const UStruct* Type = Source->GetClass();
	const FArcPersistenceSerializerInfo* Info =
		FArcSerializerRegistry::Get().FindOrDefault(Type);

	FArcJsonSaveArchive SaveAr;
	SaveAr.SetVersion(Info->Version);
	Info->SaveFunc(Source, SaveAr);

	TArray<uint8> Data = SaveAr.Finalize();

	const FString Key = UE::ArcPersistence::MakePlayerKey(PlayerId.ToString(), Domain);
	Backend->SaveEntry(Key, Data);

	// Update cache
	CachedPlayerData.FindOrAdd(PlayerId).Add(Domain, Data);
}

// ─────────────────────────────────────────────────────────────────────────────
// Descriptor-Based API
// ─────────────────────────────────────────────────────────────────────────────

void UArcPlayerPersistenceSubsystem::SavePlayerData(const FGuid& PlayerId)
{
	IArcPersistenceBackend* Backend = GetBackend();
	if (!Backend)
	{
		UE_LOG(LogArcPlayerPersistence, Warning,
			TEXT("No backend available for SavePlayerData"));
		return;
	}

	TArray<FResolvedProvider> Providers = ResolveAllProviders(PlayerId);

	Backend->BeginTransaction();

	for (const FResolvedProvider& Resolved : Providers)
	{
		SaveObjectInternal(PlayerId, Resolved.DomainPath, Resolved.Object);
	}

	// Also save legacy instance-bound providers
	if (TMap<FString, FPlayerDataProvider>* LegacyProviders = PlayerProviders.Find(PlayerId))
	{
		for (const auto& ProviderPair : *LegacyProviders)
		{
			if (ProviderPair.Value.Source.IsValid())
			{
				SaveProvider(PlayerId, ProviderPair.Value);
			}
		}
	}

	Backend->CommitTransaction();

	UE_LOG(LogArcPlayerPersistence, Log,
		TEXT("Saved %d providers for player %s"), Providers.Num(), *PlayerId.ToString());
}

void UArcPlayerPersistenceSubsystem::LoadPlayerData(const FGuid& PlayerId)
{
	IArcPersistenceBackend* Backend = GetBackend();
	if (!Backend)
	{
		UE_LOG(LogArcPlayerPersistence, Warning,
			TEXT("No backend available for LoadPlayerData"));
		return;
	}

	// Load all keys into cache — fixed domain extraction for multi-segment domains
	const FString PlayerPrefix =
		FString::Printf(TEXT("players/%s/"), *PlayerId.ToString());
	TArray<FString> Keys = Backend->ListEntries(PlayerPrefix);

	TMap<FString, TArray<uint8>>& PlayerCache = CachedPlayerData.FindOrAdd(PlayerId);
	PlayerCache.Empty();

	for (const FString& Key : Keys)
	{
		FString Domain = ExtractDomain(Key, PlayerPrefix);
		if (Domain.IsEmpty())
		{
			continue;
		}

		TArray<uint8> Data;
		if (Backend->LoadEntry(Key, Data))
		{
			PlayerCache.Add(Domain, MoveTemp(Data));
		}
	}

	UE_LOG(LogArcPlayerPersistence, Log,
		TEXT("Loaded %d domains into cache for player %s"),
		PlayerCache.Num(), *PlayerId.ToString());

	// Apply to resolved descriptor-based providers
	TArray<FResolvedProvider> Providers = ResolveAllProviders(PlayerId);
	for (const FResolvedProvider& Resolved : Providers)
	{
		if (const TArray<uint8>* Data = PlayerCache.Find(Resolved.DomainPath))
		{
			ApplyDataToObject(Resolved.Object, *Data, Resolved.DomainPath);
		}
	}

	// Apply to legacy instance-bound providers
	if (TMap<FString, FPlayerDataProvider>* LegacyProviders = PlayerProviders.Find(PlayerId))
	{
		for (auto& ProviderPair : *LegacyProviders)
		{
			if (const TArray<uint8>* Data = PlayerCache.Find(ProviderPair.Key))
			{
				if (ProviderPair.Value.Source.IsValid())
				{
					ApplyDataToProvider(ProviderPair.Value, *Data);
				}
			}
		}
	}
}

void UArcPlayerPersistenceSubsystem::SaveAllPlayerData()
{
	TSet<FGuid> AllPlayerIds;
	for (const auto& Pair : TrackedPlayers)
	{
		AllPlayerIds.Add(Pair.Key);
	}
	for (const auto& Pair : PlayerProviders)
	{
		AllPlayerIds.Add(Pair.Key);
	}

	for (const FGuid& PlayerId : AllPlayerIds)
	{
		SavePlayerData(PlayerId);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Direct API
// ─────────────────────────────────────────────────────────────────────────────

void UArcPlayerPersistenceSubsystem::SaveObject(
	const FGuid& PlayerId, const FString& Domain, UObject* Source)
{
	IArcPersistenceBackend* Backend = GetBackend();
	if (!Backend)
	{
		UE_LOG(LogArcPlayerPersistence, Warning,
			TEXT("No backend available for SaveObject"));
		return;
	}

	Backend->BeginTransaction();
	SaveObjectInternal(PlayerId, Domain, Source);
	Backend->CommitTransaction();
}

void UArcPlayerPersistenceSubsystem::LoadObject(
	const FGuid& PlayerId, const FString& Domain, UObject* Target)
{
	// Check cache first
	if (const TMap<FString, TArray<uint8>>* PlayerCache = CachedPlayerData.Find(PlayerId))
	{
		if (const TArray<uint8>* CachedData = PlayerCache->Find(Domain))
		{
			ApplyDataToObject(Target, *CachedData, Domain);
			return;
		}
	}

	// Not in cache — load from backend
	IArcPersistenceBackend* Backend = GetBackend();
	if (!Backend)
	{
		UE_LOG(LogArcPlayerPersistence, Warning,
			TEXT("No backend available for LoadObject"));
		return;
	}

	const FString Key = UE::ArcPersistence::MakePlayerKey(PlayerId.ToString(), Domain);
	TArray<uint8> Data;
	if (Backend->LoadEntry(Key, Data))
	{
		CachedPlayerData.FindOrAdd(PlayerId).Add(Domain, Data);
		ApplyDataToObject(Target, Data, Domain);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Query + Management
// ─────────────────────────────────────────────────────────────────────────────

TArray<FString> UArcPlayerPersistenceSubsystem::ListPlayerDomains(const FGuid& PlayerId)
{
	TArray<FString> Domains;

	IArcPersistenceBackend* Backend = GetBackend();
	if (!Backend)
	{
		return Domains;
	}

	const FString PlayerPrefix =
		FString::Printf(TEXT("players/%s/"), *PlayerId.ToString());
	TArray<FString> Keys = Backend->ListEntries(PlayerPrefix);

	for (const FString& Key : Keys)
	{
		FString Domain = ExtractDomain(Key, PlayerPrefix);
		if (!Domain.IsEmpty())
		{
			Domains.Add(Domain);
		}
	}

	return Domains;
}

void UArcPlayerPersistenceSubsystem::DeletePlayerDomain(
	const FGuid& PlayerId, const FString& Domain)
{
	IArcPersistenceBackend* Backend = GetBackend();
	if (!Backend)
	{
		return;
	}

	const FString Key = UE::ArcPersistence::MakePlayerKey(PlayerId.ToString(), Domain);
	Backend->DeleteEntry(Key);

	if (TMap<FString, TArray<uint8>>* PlayerCache = CachedPlayerData.Find(PlayerId))
	{
		PlayerCache->Remove(Domain);
	}
}

void UArcPlayerPersistenceSubsystem::ClearPlayerCache(const FGuid& PlayerId)
{
	CachedPlayerData.Remove(PlayerId);
}

bool UArcPlayerPersistenceSubsystem::IsPlayerDataLoaded(const FGuid& PlayerId) const
{
	return CachedPlayerData.Contains(PlayerId);
}

const TArray<uint8>* UArcPlayerPersistenceSubsystem::GetCachedData(
	const FGuid& PlayerId, const FString& Domain) const
{
	if (const TMap<FString, TArray<uint8>>* PlayerCache = CachedPlayerData.Find(PlayerId))
	{
		return PlayerCache->Find(Domain);
	}
	return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Legacy Instance-Bound API
// ─────────────────────────────────────────────────────────────────────────────

void UArcPlayerPersistenceSubsystem::RegisterPlayerDataProvider(
	const FGuid& PlayerId, const FString& Domain, UObject* Source)
{
	check(Source);
	check(PlayerId.IsValid());
	check(!Domain.IsEmpty());

	FPlayerDataProvider Provider;
	Provider.Domain = Domain;
	Provider.Source = Source;

	PlayerProviders.FindOrAdd(PlayerId).Add(Domain, Provider);

	if (const TMap<FString, TArray<uint8>>* PlayerCache = CachedPlayerData.Find(PlayerId))
	{
		if (const TArray<uint8>* Data = PlayerCache->Find(Domain))
		{
			ApplyDataToProvider(Provider, *Data);
		}
	}
}

void UArcPlayerPersistenceSubsystem::UnregisterPlayerDataProvider(
	const FGuid& PlayerId, const FString& Domain)
{
	if (TMap<FString, FPlayerDataProvider>* Providers = PlayerProviders.Find(PlayerId))
	{
		Providers->Remove(Domain);
		if (Providers->Num() == 0)
		{
			PlayerProviders.Remove(PlayerId);
		}
	}
}

void UArcPlayerPersistenceSubsystem::ApplyPlayerData(
	const FGuid& PlayerId, const FString& Domain)
{
	const TMap<FString, FPlayerDataProvider>* Providers = PlayerProviders.Find(PlayerId);
	if (!Providers)
	{
		return;
	}

	const FPlayerDataProvider* Provider = Providers->Find(Domain);
	if (!Provider || !Provider->Source.IsValid())
	{
		return;
	}

	const TMap<FString, TArray<uint8>>* PlayerCache = CachedPlayerData.Find(PlayerId);
	if (!PlayerCache)
	{
		return;
	}

	const TArray<uint8>* Data = PlayerCache->Find(Domain);
	if (!Data)
	{
		return;
	}

	ApplyDataToProvider(*Provider, *Data);
}

void UArcPlayerPersistenceSubsystem::ApplyDataToProvider(
	const FPlayerDataProvider& Provider, const TArray<uint8>& Data)
{
	UObject* Source = Provider.Source.Get();
	ApplyDataToObject(Source, Data, Provider.Domain);
}

void UArcPlayerPersistenceSubsystem::SaveProvider(
	const FGuid& PlayerId, const FPlayerDataProvider& Provider)
{
	UObject* Source = Provider.Source.Get();
	if (!Source)
	{
		return;
	}

	SaveObjectInternal(PlayerId, Provider.Domain, Source);
}
