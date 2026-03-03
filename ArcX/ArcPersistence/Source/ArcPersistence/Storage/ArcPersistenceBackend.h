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

/**
 * Abstract storage backend interface for the persistence system.
 * Implementations map logical keys to a concrete storage medium (filesystem, database, etc.).
 */
class ARCPERSISTENCE_API IArcPersistenceBackend
{
public:
	virtual ~IArcPersistenceBackend() = default;

	// Core CRUD
	virtual bool SaveEntry(const FString& Key, const TArray<uint8>& Data) = 0;
	virtual bool LoadEntry(const FString& Key, TArray<uint8>& OutData) = 0;
	virtual bool DeleteEntry(const FString& Key) = 0;
	virtual bool EntryExists(const FString& Key) = 0;

	// Prefix scan - returns all keys matching prefix
	virtual TArray<FString> ListEntries(const FString& KeyPrefix) = 0;

	// Atomic batch writes
	virtual void BeginTransaction() = 0;
	virtual void CommitTransaction() = 0;
	virtual void RollbackTransaction() = 0;

	virtual FName GetBackendName() const = 0;
};
