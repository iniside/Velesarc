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

#include "Components/ActorComponent.h"
#include "ArcPersistentIdComponent.generated.h"

/**
 * Provides stable persistence identification for World Partition actors.
 *
 * At cook time, bakes the actor's instance GUID into a serialized UPROPERTY
 * so it survives to shipping builds (where ActorGuid is editor-only).
 *
 * On BeginPlay, auto-registers the owning actor with UArcWorldPersistenceSubsystem.
 * Actors without this component can still manually call RegisterActor().
 */
UCLASS(ClassGroup=(ArcPersistence), meta=(BlueprintSpawnableComponent))
class ARCPERSISTENCE_API UArcPersistentIdComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArcPersistentIdComponent();

	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
#endif

	/** The stable persistence ID. Baked from ActorInstanceGuid at cook time. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Persistence")
	FGuid PersistenceId;
};
