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

#include "ArcCore/Conditions/ArcWorldConditionSchema.h"
#include "ArcLootWorldConditionSchema.generated.h"

/**
 * World condition schema for loot systems.
 * Extends UArcWorldConditionSchema (which already provides SelfEntity, SelfActor,
 * MassEntitySubsystem) with InstigatorEntity and InstigatorActor — the entity/actor
 * that triggered the loot roll (e.g. the player picking up loot).
 */
UCLASS(meta = (DisplayName = "Loot World Condition Schema"))
class ARCMASSITEMSRUNTIME_API UArcLootWorldConditionSchema : public UArcWorldConditionSchema
{
	GENERATED_BODY()

public:
	explicit UArcLootWorldConditionSchema(const FObjectInitializer& ObjectInitializer);

	FWorldConditionContextDataRef GetInstigatorEntityRef() const { return InstigatorEntityRef; }
	FWorldConditionContextDataRef GetInstigatorActorRef() const  { return InstigatorActorRef; }

	static FName InstigatorEntityName() { return TEXT("InstigatorEntity"); }
	static FName InstigatorActorName()  { return TEXT("InstigatorActor"); }

private:
	FWorldConditionContextDataRef InstigatorEntityRef;
	FWorldConditionContextDataRef InstigatorActorRef;
};
