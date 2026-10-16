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

#include "Mass/EntityHandle.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "Items/ArcItemTypes.h"
#include "GameplayTagContainer.h"

#include "ArcMassEquipmentFragment.generated.h"

class UArcMassEquipmentSlotPreset;
class UArcMassEquipmentSlotDefaultItems;

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassEquipmentFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FGameplayTag, FArcItemId> EquippedItems;
};

template <>
struct TMassFragmentTraits<FArcMassEquipmentFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassEquipmentSharedFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TObjectPtr<UArcMassEquipmentSlotPreset> Preset = nullptr;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UArcMassEquipmentSlotDefaultItems> Defaults = nullptr;
};

template <>
struct TMassFragmentTraits<FArcMassEquipmentSharedFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};
