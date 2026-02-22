/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
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
#include "Items/ArcItemSpec.h"
#include "Iris/ReplicationState/IrisFastArraySerializer.h"
#include "Iris/ReplicationState/Private/IrisFastArraySerializerInternal.h"
#include "Net/Serialization/FastArraySerializer.h"

#include "ArcCraftStationTypes.generated.h"

class UArcRecipeDefinition;
class UArcCraftStationComponent;

/**
 * How the crafting station tracks time for craft completion.
 */
UENUM(BlueprintType)
enum class EArcCraftStationTimeMode : uint8
{
	/** Component ticks and auto-finishes when accumulated time reaches craft duration. */
	AutoTick,
	/** Stamps UTC time on start. On interaction, checks if enough time has passed. */
	InteractionCheck
};

/**
 * A single entry in the crafting queue.
 * Replicated via Iris fast array serializer.
 */
USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcCraftQueueEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	/** Unique identifier for this queue entry. */
	UPROPERTY(BlueprintReadOnly)
	FGuid EntryId;

	/** The recipe being crafted. */
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<const UArcRecipeDefinition> Recipe = nullptr;

	/** Total number of items to produce. */
	UPROPERTY(BlueprintReadOnly)
	int32 Amount = 1;

	/** How many items have been completed so far. */
	UPROPERTY(BlueprintReadOnly)
	int32 CompletedAmount = 0;

	/** Priority — lower number = higher priority. */
	UPROPERTY(BlueprintReadOnly)
	int32 Priority = 0;

	/** UTC unix timestamp when the current item started crafting. */
	UPROPERTY(BlueprintReadOnly)
	int64 StartTimestamp = 0;

	/** Accumulated tick time for AutoTick mode (seconds). */
	UPROPERTY(BlueprintReadOnly)
	float ElapsedTickTime = 0.0f;

	/** Time mode copied from the station at queue time. */
	UPROPERTY(BlueprintReadOnly)
	EArcCraftStationTimeMode TimeMode = EArcCraftStationTimeMode::AutoTick;
};

/**
 * Replicated list of craft queue entries using Iris fast array serializer.
 */
USTRUCT()
struct ARCCRAFT_API FArcCraftStationQueue : public FIrisFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FArcCraftQueueEntry> Entries;

	using ItemArrayType = TArray<FArcCraftQueueEntry>;

	const ItemArrayType& GetItemArray() const
	{
		return Entries;
	}

	ItemArrayType& GetItemArray()
	{
		return Entries;
	}

	typedef UE::Net::TIrisFastArrayEditor<FArcCraftStationQueue> FFastArrayEditor;

	FFastArrayEditor Edit()
	{
		return FFastArrayEditor(*this);
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FArcCraftStationQueueChangedDelegate,
	UArcCraftStationComponent*, Station,
	const FArcCraftQueueEntry&, Entry);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FArcCraftStationItemCompletedDelegate,
	UArcCraftStationComponent*, Station,
	const UArcRecipeDefinition*, Recipe,
	const FArcItemSpec&, OutputSpec);
