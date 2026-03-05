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

#include "ArcPersistenceSubsystem.h"

#include "Misc/Paths.h"
#include "Storage/ArcJsonFileBackend.h"
#include "Storage/ArcSQLiteBackend.h"
#include "ArcPersistenceSettings.h"

void UArcPersistenceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const FString SaveDir = FPaths::ProjectSavedDir() / TEXT("ArcPersistence");

	const UArcPersistenceSettings* Settings = GetDefault<UArcPersistenceSettings>();
	const EArcPersistenceBackendType BackendType = Settings
		? Settings->BackendType
		: EArcPersistenceBackendType::JsonFile;

	switch (BackendType)
	{
	case EArcPersistenceBackendType::SQLite:
	{
		const FString DbPath = SaveDir / TEXT("ArcPersistence.db");
		Backend = MakeUnique<FArcSQLiteBackend>(DbPath);
		UE_LOG(LogTemp, Log, TEXT("ArcPersistence: Initialized with SQLite backend at %s"), *DbPath);
		break;
	}
	case EArcPersistenceBackendType::JsonFile:
	default:
		Backend = MakeUnique<FArcJsonFileBackend>(SaveDir);
		UE_LOG(LogTemp, Log, TEXT("ArcPersistence: Initialized with JsonFile backend at %s"), *SaveDir);
		break;
	}
}

void UArcPersistenceSubsystem::Deinitialize()
{
	if (Backend)
	{
		Backend->Flush();
	}
	Backend.Reset();
	Super::Deinitialize();
}
