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
#include "ArcPersistenceEvents.h"
#include "ArcPlayerProviderDescriptor.h"
#include "Async/Async.h"
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

TArray<uint8> UArcPlayerPersistenceSubsystem::SerializeObject(UObject* Source) const
{
	if (!Source)
	{
		return {};
	}

	const UStruct* Type = Source->GetClass();
	const FArcPersistenceSerializerInfo* Info =
		FArcSerializerRegistry::Get().FindOrDefault(Type);

	FArcJsonSaveArchive SaveAr;
	SaveAr.SetVersion(Info->Version);
	Info->SaveFunc(Source, SaveAr);

	return SaveAr.Finalize();
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

	TArray<uint8> Data = SerializeObject(Source);

	const FString Key = UE::ArcPersistence::MakePlayerKey(PlayerId.ToString(), Domain);
	Backend->SaveEntry(Key, Data).Get();

	// Update cache
	CachedPlayerData.FindOrAdd(PlayerId).Add(Domain, Data);
}

// ─────────────────────────────────────────────────────────────────────────────
// Async API
// ─────────────────────────────────────────────────────────────────────────────

TFuture<void> UArcPlayerPersistenceSubsystem::SavePlayerDataAsync(const FGuid& PlayerId)
{
	auto Promise = MakeShared<TPromise<void>>();
	TFuture<void> Future = Promise->GetFuture();

	IArcPersistenceBackend* Backend = GetBackend();
	if (!Backend)
	{
		UE_LOG(LogArcPlayerPersistence, Warning,
			TEXT("No backend available for SavePlayerDataAsync"));
		Promise->SetValue();
		return Future;
	}

	// Broadcast started event
	if (UArcPersistenceSubsystem* PersistenceSub =
		GetGameInstance()->GetSubsystem<UArcPersistenceSubsystem>())
	{
		FArcPersistenceEvent Event;
		Event.Operation = EArcPersistenceOperation::Save;
		Event.Scope = EArcPersistenceScope::Player;
		Event.ContextId = PlayerId;
		PersistenceSub->OnPersistenceStarted.Broadcast(Event);
	}

	// Game thread: resolve providers and serialize into entries
	TArray<TPair<FString, TArray<uint8>>> Entries;

	TArray<FResolvedProvider> Providers = ResolveAllProviders(PlayerId);
	for (const FResolvedProvider& Resolved : Providers)
	{
		if (Resolved.Object)
		{
			TArray<uint8> Data = SerializeObject(Resolved.Object);
			const FString Key = UE::ArcPersistence::MakePlayerKey(
				PlayerId.ToString(), Resolved.DomainPath);
			CachedPlayerData.FindOrAdd(PlayerId).Add(Resolved.DomainPath, Data);
			Entries.Emplace(Key, MoveTemp(Data));
		}
	}

	// Also serialize legacy instance-bound providers
	if (TMap<FString, FPlayerDataProvider>* LegacyProviders = PlayerProviders.Find(PlayerId))
	{
		for (const auto& ProviderPair : *LegacyProviders)
		{
			if (ProviderPair.Value.Source.IsValid())
			{
				UObject* Source = ProviderPair.Value.Source.Get();
				TArray<uint8> Data = SerializeObject(Source);
				const FString Key = UE::ArcPersistence::MakePlayerKey(
					PlayerId.ToString(), ProviderPair.Value.Domain);
				CachedPlayerData.FindOrAdd(PlayerId).Add(ProviderPair.Value.Domain, Data);
				Entries.Emplace(Key, MoveTemp(Data));
			}
		}
	}

	int32 ProviderCount = Providers.Num();

	// Async: batch save all entries
	TWeakObjectPtr<UArcPlayerPersistenceSubsystem> WeakThis(this);
	Backend->SaveEntries(MoveTemp(Entries)).Then(
		[WeakThis, Promise, PlayerId, ProviderCount](TFuture<FArcPersistenceResult> ResultFuture)
		{
			FArcPersistenceResult Result = ResultFuture.Get();
			if (!Result.bSuccess)
			{
				UE_LOG(LogArcPlayerPersistence, Warning,
					TEXT("SavePlayerDataAsync: Backend save failed for player %s: %s"),
					*PlayerId.ToString(), *Result.Error);
			}
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Promise, PlayerId, ProviderCount]()
			{
				if (UArcPlayerPersistenceSubsystem* This = WeakThis.Get())
				{
					UE_LOG(LogArcPlayerPersistence, Log,
						TEXT("Saved %d providers for player %s"), ProviderCount,
						*PlayerId.ToString());

					if (UArcPersistenceSubsystem* PersistenceSub =
						This->GetGameInstance()->GetSubsystem<UArcPersistenceSubsystem>())
					{
						FArcPersistenceEvent Event;
						Event.Operation = EArcPersistenceOperation::Save;
						Event.Scope = EArcPersistenceScope::Player;
						Event.ContextId = PlayerId;
						PersistenceSub->OnPersistenceCompleted.Broadcast(Event);
					}
				}
				Promise->SetValue();
			});
		});

	return Future;
}

TFuture<void> UArcPlayerPersistenceSubsystem::LoadPlayerDataAsync(const FGuid& PlayerId)
{
	auto Promise = MakeShared<TPromise<void>>();
	TFuture<void> Future = Promise->GetFuture();

	IArcPersistenceBackend* Backend = GetBackend();
	if (!Backend)
	{
		UE_LOG(LogArcPlayerPersistence, Warning,
			TEXT("No backend available for LoadPlayerDataAsync"));
		Promise->SetValue();
		return Future;
	}

	// Broadcast started event
	if (UArcPersistenceSubsystem* PersistenceSub =
		GetGameInstance()->GetSubsystem<UArcPersistenceSubsystem>())
	{
		FArcPersistenceEvent Event;
		Event.Operation = EArcPersistenceOperation::Load;
		Event.Scope = EArcPersistenceScope::Player;
		Event.ContextId = PlayerId;
		PersistenceSub->OnPersistenceStarted.Broadcast(Event);
	}

	const FString PlayerPrefix =
		FString::Printf(TEXT("players/%s/"), *PlayerId.ToString());

	TWeakObjectPtr<UArcPlayerPersistenceSubsystem> WeakThis(this);

	// Async: list entries then load on background thread, apply on game thread
	TFuture<FArcPersistenceListResult> ListFuture = Backend->ListEntries(PlayerPrefix);

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
		[WeakThis, Promise, PlayerId, PlayerPrefix, Backend, ListFuture = MoveTemp(ListFuture)]() mutable
	{
		FArcPersistenceListResult ListResult = ListFuture.Get();
		if (!ListResult.bSuccess)
		{
			AsyncTask(ENamedThreads::GameThread, [Promise]()
			{
				Promise->SetValue();
			});
			return;
		}

		// Background: load each entry serially
		TArray<TPair<FString, TArray<uint8>>> LoadedEntries;
		for (const FString& Key : ListResult.Keys)
		{
			FString Domain = ExtractDomain(Key, PlayerPrefix);
			if (Domain.IsEmpty())
			{
				continue;
			}

			FArcPersistenceLoadResult LoadResult = Backend->LoadEntry(Key).Get();
			if (LoadResult.bSuccess)
			{
				LoadedEntries.Emplace(Domain, MoveTemp(LoadResult.Data));
			}
		}

		// Game thread continuation: populate cache and apply to providers
		AsyncTask(ENamedThreads::GameThread,
			[WeakThis, Promise, PlayerId, Entries = MoveTemp(LoadedEntries)]() mutable
		{
				if (UArcPlayerPersistenceSubsystem* This = WeakThis.Get())
				{
					TMap<FString, TArray<uint8>>& PlayerCache =
						This->CachedPlayerData.FindOrAdd(PlayerId);
					PlayerCache.Empty();

					for (auto& Entry : Entries)
					{
						PlayerCache.Add(Entry.Key, MoveTemp(Entry.Value));
					}

					UE_LOG(LogArcPlayerPersistence, Log,
						TEXT("Loaded %d domains into cache for player %s"),
						PlayerCache.Num(), *PlayerId.ToString());

					// Apply to resolved descriptor-based providers
					TArray<FResolvedProvider> Providers =
						This->ResolveAllProviders(PlayerId);
					for (const FResolvedProvider& Resolved : Providers)
					{
						if (const TArray<uint8>* Data =
							PlayerCache.Find(Resolved.DomainPath))
						{
							This->ApplyDataToObject(
								Resolved.Object, *Data, Resolved.DomainPath);
						}
					}

					// Apply to legacy instance-bound providers
					if (TMap<FString, FPlayerDataProvider>* LegacyProviders =
						This->PlayerProviders.Find(PlayerId))
					{
						for (auto& ProviderPair : *LegacyProviders)
						{
							if (const TArray<uint8>* Data =
								PlayerCache.Find(ProviderPair.Key))
							{
								if (ProviderPair.Value.Source.IsValid())
								{
									This->ApplyDataToProvider(
										ProviderPair.Value, *Data);
								}
							}
						}
					}

					// Broadcast completed event
					if (UArcPersistenceSubsystem* PersistenceSub =
						This->GetGameInstance()->GetSubsystem<UArcPersistenceSubsystem>())
					{
						FArcPersistenceEvent Event;
						Event.Operation = EArcPersistenceOperation::Load;
						Event.Scope = EArcPersistenceScope::Player;
						Event.ContextId = PlayerId;
						PersistenceSub->OnPersistenceCompleted.Broadcast(Event);
					}
				}
				Promise->SetValue();
			});
		});

	return Future;
}

TFuture<void> UArcPlayerPersistenceSubsystem::SaveAllPlayerDataAsync()
{
	auto Promise = MakeShared<TPromise<void>>();
	TFuture<void> Future = Promise->GetFuture();

	IArcPersistenceBackend* Backend = GetBackend();
	if (!Backend)
	{
		UE_LOG(LogArcPlayerPersistence, Warning,
			TEXT("No backend available for SaveAllPlayerDataAsync"));
		Promise->SetValue();
		return Future;
	}

	// Collect all player IDs
	TSet<FGuid> AllPlayerIds;
	for (const auto& Pair : TrackedPlayers)
	{
		AllPlayerIds.Add(Pair.Key);
	}
	for (const auto& Pair : PlayerProviders)
	{
		AllPlayerIds.Add(Pair.Key);
	}

	// Game thread: serialize ALL providers for all players into one batch
	TArray<TPair<FString, TArray<uint8>>> AllEntries;

	for (const FGuid& PId : AllPlayerIds)
	{
		TArray<FResolvedProvider> Providers = ResolveAllProviders(PId);
		for (const FResolvedProvider& Resolved : Providers)
		{
			if (Resolved.Object)
			{
				TArray<uint8> Data = SerializeObject(Resolved.Object);
				const FString Key = UE::ArcPersistence::MakePlayerKey(
					PId.ToString(), Resolved.DomainPath);
				CachedPlayerData.FindOrAdd(PId).Add(Resolved.DomainPath, Data);
				AllEntries.Emplace(Key, MoveTemp(Data));
			}
		}

		// Legacy providers
		if (TMap<FString, FPlayerDataProvider>* LegacyProviders = PlayerProviders.Find(PId))
		{
			for (const auto& ProviderPair : *LegacyProviders)
			{
				if (ProviderPair.Value.Source.IsValid())
				{
					UObject* Source = ProviderPair.Value.Source.Get();
					TArray<uint8> Data = SerializeObject(Source);
					const FString Key = UE::ArcPersistence::MakePlayerKey(
						PId.ToString(), ProviderPair.Value.Domain);
					CachedPlayerData.FindOrAdd(PId).Add(ProviderPair.Value.Domain, Data);
					AllEntries.Emplace(Key, MoveTemp(Data));
				}
			}
		}
	}

	int32 EntryCount = AllEntries.Num();
	int32 PlayerCount = AllPlayerIds.Num();

	// Single batch save
	TWeakObjectPtr<UArcPlayerPersistenceSubsystem> WeakThis(this);
	Backend->SaveEntries(MoveTemp(AllEntries)).Then(
		[WeakThis, Promise, EntryCount, PlayerCount](TFuture<FArcPersistenceResult> ResultFuture)
		{
			FArcPersistenceResult Result = ResultFuture.Get();
			if (!Result.bSuccess)
			{
				UE_LOG(LogArcPlayerPersistence, Warning,
					TEXT("SaveAllPlayerDataAsync: Backend save failed: %s"), *Result.Error);
			}
			AsyncTask(ENamedThreads::GameThread,
				[WeakThis, Promise, EntryCount, PlayerCount]()
			{
				UE_LOG(LogArcPlayerPersistence, Log,
					TEXT("SaveAllPlayerDataAsync: Saved %d entries for %d players"),
					EntryCount, PlayerCount);
				Promise->SetValue();
			});
		});

	return Future;
}

TFuture<void> UArcPlayerPersistenceSubsystem::SaveObjectAsync(
	const FGuid& PlayerId, const FString& Domain, UObject* Source)
{
	auto Promise = MakeShared<TPromise<void>>();
	TFuture<void> Future = Promise->GetFuture();

	if (!Source)
	{
		Promise->SetValue();
		return Future;
	}

	IArcPersistenceBackend* Backend = GetBackend();
	if (!Backend)
	{
		UE_LOG(LogArcPlayerPersistence, Warning,
			TEXT("No backend available for SaveObjectAsync"));
		Promise->SetValue();
		return Future;
	}

	// Game thread: serialize
	TArray<uint8> Data = SerializeObject(Source);
	const FString Key = UE::ArcPersistence::MakePlayerKey(PlayerId.ToString(), Domain);

	// Update cache immediately
	CachedPlayerData.FindOrAdd(PlayerId).Add(Domain, Data);

	// Async: save entry
	TWeakObjectPtr<UArcPlayerPersistenceSubsystem> WeakThis(this);
	Backend->SaveEntry(Key, MoveTemp(Data)).Then(
		[Promise, Domain](TFuture<FArcPersistenceResult> ResultFuture)
		{
			FArcPersistenceResult Result = ResultFuture.Get();
			if (!Result.bSuccess)
			{
				UE_LOG(LogArcPlayerPersistence, Warning,
					TEXT("SaveObjectAsync: Backend save failed for domain '%s': %s"),
					*Domain, *Result.Error);
			}
			AsyncTask(ENamedThreads::GameThread, [Promise]()
			{
				Promise->SetValue();
			});
		});

	return Future;
}

TFuture<void> UArcPlayerPersistenceSubsystem::LoadObjectAsync(
	const FGuid& PlayerId, const FString& Domain, UObject* Target)
{
	auto Promise = MakeShared<TPromise<void>>();
	TFuture<void> Future = Promise->GetFuture();

	if (!Target)
	{
		Promise->SetValue();
		return Future;
	}

	// Check cache first (game thread, no async needed)
	if (const TMap<FString, TArray<uint8>>* PlayerCache = CachedPlayerData.Find(PlayerId))
	{
		if (const TArray<uint8>* CachedBytes = PlayerCache->Find(Domain))
		{
			ApplyDataToObject(Target, *CachedBytes, Domain);
			Promise->SetValue();
			return Future;
		}
	}

	// Cache miss — load from backend
	IArcPersistenceBackend* Backend = GetBackend();
	if (!Backend)
	{
		UE_LOG(LogArcPlayerPersistence, Warning,
			TEXT("No backend available for LoadObjectAsync"));
		Promise->SetValue();
		return Future;
	}

	const FString Key = UE::ArcPersistence::MakePlayerKey(PlayerId.ToString(), Domain);
	TWeakObjectPtr<UObject> WeakTarget(Target);
	TWeakObjectPtr<UArcPlayerPersistenceSubsystem> WeakThis(this);

	Backend->LoadEntry(Key).Then(
		[WeakThis, WeakTarget, Promise, PlayerId, Domain](
			TFuture<FArcPersistenceLoadResult> LoadFuture)
		{
			FArcPersistenceLoadResult LoadResult = LoadFuture.Get();
			AsyncTask(ENamedThreads::GameThread,
				[WeakThis, WeakTarget, Promise, PlayerId, Domain,
				 Data = MoveTemp(LoadResult.Data), bSuccess = LoadResult.bSuccess]()
			{
				if (bSuccess)
				{
					if (UArcPlayerPersistenceSubsystem* This = WeakThis.Get())
					{
						This->CachedPlayerData.FindOrAdd(PlayerId).Add(Domain, Data);

						if (UObject* Obj = WeakTarget.Get())
						{
							This->ApplyDataToObject(Obj, Data, Domain);
						}
					}
				}
				Promise->SetValue();
			});
		});

	return Future;
}

// ─────────────────────────────────────────────────────────────────────────────
// Descriptor-Based API (sync wrappers)
// ─────────────────────────────────────────────────────────────────────────────

void UArcPlayerPersistenceSubsystem::SavePlayerData(const FGuid& PlayerId)
{
	SavePlayerDataAsync(PlayerId).Get();
}

void UArcPlayerPersistenceSubsystem::LoadPlayerData(const FGuid& PlayerId)
{
	LoadPlayerDataAsync(PlayerId).Get();
}

void UArcPlayerPersistenceSubsystem::SaveAllPlayerData()
{
	SaveAllPlayerDataAsync().Get();
}

// ─────────────────────────────────────────────────────────────────────────────
// Direct API (sync wrappers)
// ─────────────────────────────────────────────────────────────────────────────

void UArcPlayerPersistenceSubsystem::SaveObject(
	const FGuid& PlayerId, const FString& Domain, UObject* Source)
{
	SaveObjectAsync(PlayerId, Domain, Source).Get();
}

void UArcPlayerPersistenceSubsystem::LoadObject(
	const FGuid& PlayerId, const FString& Domain, UObject* Target)
{
	LoadObjectAsync(PlayerId, Domain, Target).Get();
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
	FArcPersistenceListResult ListResult = Backend->ListEntries(PlayerPrefix).Get();

	if (ListResult.bSuccess)
	{
		for (const FString& Key : ListResult.Keys)
		{
			FString Domain = ExtractDomain(Key, PlayerPrefix);
			if (!Domain.IsEmpty())
			{
				Domains.Add(Domain);
			}
		}
	}

	return Domains;
}

void UArcPlayerPersistenceSubsystem::DeletePlayerDomain(
	const FGuid& PlayerId, const FString& Domain)
{
	// Update cache immediately on game thread
	if (TMap<FString, TArray<uint8>>* PlayerCache = CachedPlayerData.Find(PlayerId))
	{
		PlayerCache->Remove(Domain);
	}

	IArcPersistenceBackend* Backend = GetBackend();
	if (!Backend)
	{
		return;
	}

	const FString Key = UE::ArcPersistence::MakePlayerKey(PlayerId.ToString(), Domain);

	// Fire-and-forget async delete
	Backend->DeleteEntry(Key);
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
