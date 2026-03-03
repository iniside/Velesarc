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
#include "Storage/ArcPersistenceBackend.h"

#include "ArcPersistenceSubsystem.generated.h"

/**
 * Base persistence subsystem — owns the storage backend.
 * World and Player subsystems query this for backend access.
 *
 * GameInstanceSubsystem so it survives map transitions and is available
 * before worlds load.
 */
UCLASS()
class ARCPERSISTENCE_API UArcPersistenceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Get the active storage backend. May be null before Initialize. */
	IArcPersistenceBackend* GetBackend() const { return Backend.Get(); }

private:
	TUniquePtr<IArcPersistenceBackend> Backend;
};
