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

#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "GameplayTagContainer.h"
#include "Items/ArcItemSpec.h"
#include "ArcCraft/Station/ArcCraftStationTypes.h"

#include "ArcCraftMassFragments.generated.h"

/**
 * Shared config for crafting station entities. Immutable after creation.
 * Deduplicates across entities sharing the same station configuration.
 */
USTRUCT(BlueprintType, meta = (Tooltip = "Shared immutable config for crafting station entities; deduplicates across entities sharing the same station configuration."))
struct ARCCRAFT_API FArcCraftStationConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	/** Tags identifying this station *type* (e.g. "Station.Forge", "Station.Hand").
	 *  Recipe RequiredStationTags must be a subset of these. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	FGameplayTagContainer StationTags;

	/** Recipe tier/category tags this station offers (matched against recipe RecipeTags). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	FGameplayTagContainer OfferedRecipeTags;

	/** How time is tracked for crafting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	EArcCraftStationTimeMode TimeMode = EArcCraftStationTimeMode::AutoTick;

	/** Tick interval for AutoTick mode (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station", meta = (ClampMin = "0.1"))
	float TickInterval = 0.5f;

	/** Queue entries that craft simultaneously on this station. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station", meta = (ClampMin = "1"))
	int32 ConcurrentCraftSlots = 1;

	/** Queue length per slot. 0 = unlimited. Station capacity is the product of both. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station", meta = (ClampMin = "0"))
	int32 QueueLengthPerSlot = 8;

	int32 GetQueueCapacity() const
	{
		return QueueLengthPerSlot > 0 ? ConcurrentCraftSlots * QueueLengthPerSlot : 0;
	}
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
USTRUCT(meta = (Tooltip = "Items deposited into the crafting station as ingredients."))
struct ARCCRAFT_API FArcCraftInputFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(meta = (Tooltip = "Items deposited into the crafting station as ingredients."))
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
USTRUCT(meta = (Tooltip = "Completed crafted items waiting to be picked up."))
struct ARCCRAFT_API FArcCraftOutputFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(meta = (Tooltip = "Completed crafted items waiting to be picked up."))
	TArray<FArcItemSpec> OutputItems;

	/** Set by craft tick processor when new output is added. Cleared by consumers. */
	bool bOutputDirty = false;
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
USTRUCT(meta = (Tooltip = "The crafting queue on an entity; replaces FArcCraftStationQueue for entity-backed stations."))
struct ARCCRAFT_API FArcCraftQueueFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Queue entries. Per-entry progress lives on the entry; up to
	 *  FArcCraftStationConfigFragment::ConcurrentCraftSlots may be Active at once. */
	UPROPERTY()
	TArray<FArcCraftQueueEntry> Entries;
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
USTRUCT(meta = (Tooltip = "Tag identifying an entity as a crafting station."))
struct ARCCRAFT_API FArcCraftStationTag : public FMassTag
{
	GENERATED_BODY()
};

/**
 * Tag added to craft station entities that use AutoTick time mode.
 * The tick processor requires this tag — entities without it (InteractionCheck mode)
 * are never iterated, avoiding wasted processing.
 */
USTRUCT(meta = (Tooltip = "Tag added to craft station entities that use AutoTick time mode; entities without it are never iterated by the tick processor."))
struct ARCCRAFT_API FArcCraftAutoTickTag : public FMassTag
{
	GENERATED_BODY()
};
