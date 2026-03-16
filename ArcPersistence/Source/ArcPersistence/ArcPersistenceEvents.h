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
#include "ArcPersistenceEvents.generated.h"

UENUM(BlueprintType)
enum class EArcPersistenceOperation : uint8
{
	Save,
	Load
};

UENUM(BlueprintType)
enum class EArcPersistenceScope : uint8
{
	World,
	Player
};

USTRUCT(BlueprintType)
struct ARCPERSISTENCE_API FArcPersistenceEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	EArcPersistenceOperation Operation = EArcPersistenceOperation::Save;

	UPROPERTY(BlueprintReadOnly)
	EArcPersistenceScope Scope = EArcPersistenceScope::World;

	UPROPERTY(BlueprintReadOnly)
	FGuid ContextId;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FArcPersistenceEventDelegate, const FArcPersistenceEvent&);
