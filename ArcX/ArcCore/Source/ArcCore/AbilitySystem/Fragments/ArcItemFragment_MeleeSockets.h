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

#include "Items/Fragments/ArcItemFragment.h"
#include "ArcItemFragment_MeleeSockets.generated.h"

/**
 * Configures per-weapon melee trace geometry: socket names for the sweep endpoints,
 * trace radius, collision channel, and whether to use the character mesh or weapon mesh.
 */
USTRUCT(BlueprintType, meta = (Category = "Melee"))
struct ARCCORE_API FArcItemFragment_MeleeSockets : public FArcItemFragment
{
	GENERATED_BODY()

public:
	/** Socket at the tip/end of the weapon (e.g. "blade_tip"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName TipSocketName;

	/** Socket at the base/hilt of the weapon (e.g. "blade_base"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName BaseSocketName;

	/** Sphere sweep radius around each trace line. 0 = line trace. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float TraceRadius = 0.f;

	/** Collision channel for the sweep. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/**
	 * If true, read sockets from IArcSkeletalMeshOwnerInterface on the avatar actor.
	 * If false (default), resolve weapon mesh via ItemAttachmentComponent.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bUseCharacterMesh = false;
};
