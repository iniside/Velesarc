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
#include "Engine/GameInstance.h"
#include "Serialization/ArcSerializerRegistry.h"
#include "Serialization/ArcReflectionSerializer.h"
#include "Serialization/ArcJsonSaveArchive.h"
#include "Serialization/ArcJsonLoadArchive.h"
#include "Storage/ArcPersistenceBackend.h"
#include "Subsystems/SubsystemCollection.h"

void UArcPlayerPersistenceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency<UArcPersistenceSubsystem>();
}

void UArcPlayerPersistenceSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
// Core API
// ─────────────────────────────────────────────────────────────────────────────

void UArcPlayerPersistenceSubsystem::LoadPlayerData(const FGuid& PlayerId)
{
	IArcPersistenceBackend* Backend = GetGameInstance()->GetSubsystem<UArcPersistenceSubsystem>()->GetBackend();
	if (!Backend)
	{
		UE_LOG(LogTemp, Warning, TEXT("ArcPlayerPersistence: No backend available for LoadPlayerData"));
		return;
	}

	const FString Prefix = FString::Printf(TEXT("players/%s"), *PlayerId.ToString());
	TArray<FString> Keys = Backend->ListEntries(Prefix);

	TMap<FString, TArray<uint8>>& PlayerCache = CachedPlayerData.FindOrAdd(PlayerId);
	PlayerCache.Empty();

	for (const FString& Key : Keys)
	{
		// Key format: "players/{guid}/{domain}"
		// Extract domain from the last path segment
		FString Domain;
		if (!Key.Split(TEXT("/"), nullptr, &Domain, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
		{
			continue;
		}

		TArray<uint8> Data;
		if (Backend->LoadEntry(Key, Data))
		{
			PlayerCache.Add(Domain, MoveTemp(Data));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("ArcPlayerPersistence: Loaded %d domains for player %s"),
		PlayerCache.Num(), *PlayerId.ToString());

	// If providers are already registered, apply data now
	if (TMap<FString, FPlayerDataProvider>* Providers = PlayerProviders.Find(PlayerId))
	{
		for (auto& ProviderPair : *Providers)
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

void UArcPlayerPersistenceSubsystem::SavePlayerData(const FGuid& PlayerId)
{
	IArcPersistenceBackend* Backend = GetGameInstance()->GetSubsystem<UArcPersistenceSubsystem>()->GetBackend();
	if (!Backend)
	{
		UE_LOG(LogTemp, Warning, TEXT("ArcPlayerPersistence: No backend available for SavePlayerData"));
		return;
	}

	TMap<FString, FPlayerDataProvider>* Providers = PlayerProviders.Find(PlayerId);
	if (!Providers)
	{
		return;
	}

	Backend->BeginTransaction();

	for (const auto& ProviderPair : *Providers)
	{
		if (ProviderPair.Value.Source.IsValid())
		{
			SaveProvider(PlayerId, ProviderPair.Value);
		}
	}

	Backend->CommitTransaction();

	UE_LOG(LogTemp, Log, TEXT("ArcPlayerPersistence: Saved %d domains for player %s"),
		Providers->Num(), *PlayerId.ToString());
}

void UArcPlayerPersistenceSubsystem::SaveAllPlayerData()
{
	for (const auto& PlayerPair : PlayerProviders)
	{
		SavePlayerData(PlayerPair.Key);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Data Provider Registration
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

	// If cached data exists for this domain, apply immediately
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

void UArcPlayerPersistenceSubsystem::ApplyPlayerData(const FGuid& PlayerId, const FString& Domain)
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

bool UArcPlayerPersistenceSubsystem::IsPlayerDataLoaded(const FGuid& PlayerId) const
{
	return CachedPlayerData.Contains(PlayerId);
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal: Serialization
// ─────────────────────────────────────────────────────────────────────────────

void UArcPlayerPersistenceSubsystem::ApplyDataToProvider(
	const FPlayerDataProvider& Provider, const TArray<uint8>& Data)
{
	UObject* Source = Provider.Source.Get();
	if (!Source)
	{
		return;
	}

	const UStruct* Type = Source->GetClass();
	const FArcPersistenceSerializerInfo* SerializerInfo = FArcSerializerRegistry::Get().Find(Type);

	FArcJsonLoadArchive LoadAr;
	if (!LoadAr.InitializeFromData(Data))
	{
		UE_LOG(LogTemp, Warning, TEXT("ArcPlayerPersistence: Failed to parse data for domain '%s'"),
			*Provider.Domain);
		return;
	}

	// Version check
	uint32 ExpectedVersion;
	if (SerializerInfo)
	{
		ExpectedVersion = SerializerInfo->Version;
	}
	else
	{
		ExpectedVersion = FArcReflectionSerializer::ComputeSchemaVersion(Type);
	}

	if (LoadAr.GetVersion() != ExpectedVersion)
	{
		UE_LOG(LogTemp, Log,
			TEXT("ArcPlayerPersistence: Version mismatch for domain '%s' (saved: %u, current: %u) — discarding"),
			*Provider.Domain, LoadAr.GetVersion(), ExpectedVersion);
		return;
	}

	if (SerializerInfo)
	{
		SerializerInfo->LoadFunc(Source, LoadAr);
	}
	else
	{
		FArcReflectionSerializer::Load(Type, Source, LoadAr);
	}
}

void UArcPlayerPersistenceSubsystem::SaveProvider(
	const FGuid& PlayerId, const FPlayerDataProvider& Provider)
{
	UObject* Source = Provider.Source.Get();
	if (!Source)
	{
		return;
	}

	IArcPersistenceBackend* Backend = GetGameInstance()->GetSubsystem<UArcPersistenceSubsystem>()->GetBackend();
	if (!Backend)
	{
		return;
	}

	const UStruct* Type = Source->GetClass();
	const FArcPersistenceSerializerInfo* SerializerInfo = FArcSerializerRegistry::Get().Find(Type);

	FArcJsonSaveArchive SaveAr;

	if (SerializerInfo)
	{
		SaveAr.SetVersion(SerializerInfo->Version);
		SerializerInfo->SaveFunc(Source, SaveAr);
	}
	else
	{
		uint32 SchemaVersion = FArcReflectionSerializer::ComputeSchemaVersion(Type);
		SaveAr.SetVersion(SchemaVersion);
		FArcReflectionSerializer::Save(Type, Source, SaveAr);
	}

	TArray<uint8> Data = SaveAr.Finalize();

	const FString Key = FString::Printf(TEXT("players/%s/%s"),
		*PlayerId.ToString(), *Provider.Domain);
	Backend->SaveEntry(Key, Data);

	// Update cache
	CachedPlayerData.FindOrAdd(PlayerId).Add(Provider.Domain, Data);
}
