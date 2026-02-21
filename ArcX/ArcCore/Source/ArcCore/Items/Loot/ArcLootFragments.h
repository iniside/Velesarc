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

#include "MassEntityTypes.h"
#include "Items/ArcItemSpec.h"
#include "ArcLootFragments.generated.h"

class UArcLootTable;

/** Per-entity mutable fragment holding loot items. */
USTRUCT()
struct ARCCORE_API FArcLootContainerFragment : public FMassFragment
{
	GENERATED_BODY()

	TArray<FArcItemSpec> Items;
};

template<>
struct TMassFragmentTraits<FArcLootContainerFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

/** Tag marking entities that are loot containers (dropped loot on ground). */
USTRUCT()
struct ARCCORE_API FArcLootContainerTag : public FMassTag
{
	GENERATED_BODY()
};

/** Const shared fragment: loot table config for entities that can drop loot. */
USTRUCT(BlueprintType)
struct ARCCORE_API FArcLootTableConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Loot")
	TSoftObjectPtr<UArcLootTable> LootTable;
};

template<>
struct TMassFragmentTraits<FArcLootTableConfigFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

/** Tag marking entities that have a loot table assigned (e.g., enemies). */
USTRUCT()
struct ARCCORE_API FArcLootSourceTag : public FMassTag
{
	GENERATED_BODY()
};
