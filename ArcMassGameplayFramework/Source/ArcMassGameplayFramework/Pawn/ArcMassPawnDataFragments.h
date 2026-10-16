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

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"

#include "ArcMassPawnDataFragments.generated.h"

class APawn;
class APlayerState;
class APlayerController;
class UArcMassPawnExtensionComponent;
struct FMassEntityManager;

/**
 * FArcMassAddPawnComponent
 *
 * Polymorphic descriptor used by UArcMassPawnData to add an actor component to
 * the pawn during DataInitialized. Subclass and override GiveComponent.
 */
USTRUCT(BlueprintType)
struct ARCMASSGAMEPLAYFRAMEWORK_API FArcMassAddPawnComponent
{
	GENERATED_BODY()

	virtual ~FArcMassAddPawnComponent() = default;

	virtual void GiveComponent(APawn* Pawn) const
	{
	}
};

/**
 * FArcMassPawnInitFragment
 *
 * Initialization step run on the pawn actor during PawnExtension's DataInitialized
 * transition. Receives optional Mass entity context so init code can cross over
 * to the entity if it chooses to, but the entity may be invalid for non-Mass pawns.
 */
USTRUCT(BlueprintType)
struct ARCMASSGAMEPLAYFRAMEWORK_API FArcMassPawnInitFragment
{
	GENERATED_BODY()

	virtual ~FArcMassPawnInitFragment() = default;

	virtual void Initialize(APawn* Pawn
							, APlayerState* PlayerState
							, APlayerController* PlayerController
							, FMassEntityManager* EntityManager
							, FMassEntityHandle Entity
							, UArcMassPawnExtensionComponent* OwnerExtension) const
	{
	}
};
