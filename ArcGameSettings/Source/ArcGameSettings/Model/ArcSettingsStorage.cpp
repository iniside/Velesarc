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

#include "Model/ArcSettingsStorage.h"

#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

namespace ArcSettingsStorage_Internal
{
	static FString GetLocalStorePath(const FString& SlotName)
	{
		return FPaths::ProjectSavedDir() / TEXT("Settings") / SlotName + TEXT(".sav");
	}
}

// ---- UArcSettingsSharedSaveGame ----

void UArcSettingsSharedSaveGame::StorePropertyBag(FInstancedPropertyBag& InBag)
{
	PropertyBagData.Reset();
	FMemoryWriter MemWriter(PropertyBagData, /*bIsPersistent=*/true);
	FObjectAndNameAsStringProxyArchive Ar(MemWriter, /*bInLoadIfFindFails=*/false);
	InBag.Serialize(Ar);
}

bool UArcSettingsSharedSaveGame::RestorePropertyBag(FInstancedPropertyBag& OutBag) const
{
	if (PropertyBagData.IsEmpty())
	{
		return false;
	}

	FMemoryReader MemReader(PropertyBagData, /*bIsPersistent=*/true);
	FObjectAndNameAsStringProxyArchive Ar(MemReader, /*bInLoadIfFindFails=*/true);
	OutBag.Serialize(Ar);
	return true;
}

// ---- ArcSettingsStorage namespace functions ----

bool ArcSettingsStorage::SaveLocalStore(FInstancedPropertyBag& InBag, const FString& InSlotName)
{
	TArray<uint8> RawData;
	FMemoryWriter MemWriter(RawData, /*bIsPersistent=*/true);
	FObjectAndNameAsStringProxyArchive Ar(MemWriter, /*bInLoadIfFindFails=*/false);
	InBag.Serialize(Ar);

	const FString FilePath = ArcSettingsStorage_Internal::GetLocalStorePath(InSlotName);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), /*Tree=*/true);
	return FFileHelper::SaveArrayToFile(RawData, *FilePath);
}

bool ArcSettingsStorage::LoadLocalStore(FInstancedPropertyBag& OutBag, const FString& InSlotName)
{
	const FString FilePath = ArcSettingsStorage_Internal::GetLocalStorePath(InSlotName);
	if (!IFileManager::Get().FileExists(*FilePath))
	{
		return false;
	}

	TArray<uint8> RawData;
	if (!FFileHelper::LoadFileToArray(RawData, *FilePath))
	{
		return false;
	}

	FMemoryReader MemReader(RawData, /*bIsPersistent=*/true);
	FObjectAndNameAsStringProxyArchive Ar(MemReader, /*bInLoadIfFindFails=*/true);
	OutBag.Serialize(Ar);
	return true;
}
