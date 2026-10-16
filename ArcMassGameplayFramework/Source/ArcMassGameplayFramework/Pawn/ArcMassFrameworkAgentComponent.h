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

#pragma once

#include "MassAgentComponent.h"
// CLAUDE.md: include directly; MassEntityTypes.h does not re-export FMassEntityHandle on ue5-main.
#include "Mass/EntityHandle.h"

#include "ArcMassFrameworkAgentComponent.generated.h"

/**
 * Thin UMassAgentComponent subclass for ArcMassGameplayFramework. The pawn-side init
 * chain on UArcMassPawnExtensionComponent gates Spawned -> DataAvailable on a valid
 * entity handle, so when SetEntityHandle assigns one we forward directly into the
 * sibling PawnExtension's CheckDefaultInitialization. Engine FMassActorFragment
 * handles the actor<->entity round-trip; no custom fragment introduced.
 */
UCLASS(ClassGroup = "ArcMass", meta = (BlueprintSpawnableComponent))
class ARCMASSGAMEPLAYFRAMEWORK_API UArcMassFrameworkAgentComponent : public UMassAgentComponent
{
	GENERATED_BODY()

public:
	UArcMassFrameworkAgentComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void SetEntityHandle(const FMassEntityHandle NewHandle) override;

private:
	/** True when BeginPlay has successfully seeded an entity via UArcMassSpawnerSubsystem. */
	bool bSpawnerSeeded = false;

	/** Handle returned by SpawnForAgentComponent; used for teardown in EndPlay. */
	FMassEntityHandle SpawnedHandle;
};
