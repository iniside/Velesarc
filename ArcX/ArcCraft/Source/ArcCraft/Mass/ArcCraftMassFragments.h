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
#include "GameplayTagContainer.h"
#include "Items/ArcItemSpec.h"
#include "ArcCraft/Station/ArcCraftStationTypes.h"

#include "ArcCraftMassFragments.generated.h"

/**
 * Shared config for crafting station entities. Immutable after creation.
 * Deduplicates across entities sharing the same station configuration.
 */
USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcCraftStationConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	/** Tags identifying this station type (e.g. "Station.Forge"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	FGameplayTagContainer StationTags;

	/** How time is tracked for crafting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	EArcCraftStationTimeMode TimeMode = EArcCraftStationTimeMode::AutoTick;

	/** Tick interval for AutoTick mode (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station", meta = (ClampMin = "0.1"))
	float TickInterval = 0.5f;

	/** Maximum entries in the craft queue. 0 = unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station", meta = (ClampMin = "0"))
	int32 MaxQueueSize = 5;
};

template<>
struct TMassFragmentTraits<FArcCraftStationConfigFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

// ---------------------------------------------------------------------------

/** Items deposited into the crafting station as ingredients. */
USTRUCT()
struct ARCCRAFT_API FArcCraftInputFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FArcItemSpec> InputItems;
};

template<>
struct TMassFragmentTraits<FArcCraftInputFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

// ---------------------------------------------------------------------------

/** Completed crafted items waiting to be picked up. */
USTRUCT()
struct ARCCRAFT_API FArcCraftOutputFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FArcItemSpec> OutputItems;
};

template<>
struct TMassFragmentTraits<FArcCraftOutputFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

// ---------------------------------------------------------------------------

/** The crafting queue on an entity. Replaces FArcCraftStationQueue for entity-backed stations. */
USTRUCT()
struct ARCCRAFT_API FArcCraftQueueFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Active queue entries. */
	UPROPERTY()
	TArray<FArcCraftQueueEntry> Entries;

	/** Index of the currently active entry (lowest priority). INDEX_NONE if idle. */
	int32 ActiveEntryIndex = INDEX_NONE;
};

template<>
struct TMassFragmentTraits<FArcCraftQueueFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

// ---------------------------------------------------------------------------

/** Tag identifying an entity as a crafting station. */
USTRUCT()
struct ARCCRAFT_API FArcCraftStationTag : public FMassTag
{
	GENERATED_BODY()
};

/**
 * Tag added to craft station entities that use AutoTick time mode.
 * The tick processor requires this tag — entities without it (InteractionCheck mode)
 * are never iterated, avoiding wasted processing.
 */
USTRUCT()
struct ARCCRAFT_API FArcCraftAutoTickTag : public FMassTag
{
	GENERATED_BODY()
};
