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

#include "CoreMinimal.h"
#include "Storage/ArcPersistenceResult.h"

template<typename T> class TFuture;

class ARCPERSISTENCE_API IArcPersistenceBackend
{
public:
	virtual ~IArcPersistenceBackend() = default;

	virtual TFuture<FArcPersistenceResult> SaveEntry(const FString& Key, TArray<uint8> Data) = 0;
	virtual TFuture<FArcPersistenceLoadResult> LoadEntry(const FString& Key) = 0;
	virtual TFuture<FArcPersistenceResult> DeleteEntry(const FString& Key) = 0;
	virtual TFuture<FArcPersistenceResult> EntryExists(const FString& Key) = 0;
	virtual TFuture<FArcPersistenceListResult> ListEntries(const FString& KeyPrefix) = 0;
	virtual TFuture<FArcPersistenceResult> SaveEntries(TArray<TPair<FString, TArray<uint8>>> Entries) = 0;

	virtual FName GetBackendName() const = 0;
	virtual void Flush() = 0;
};
