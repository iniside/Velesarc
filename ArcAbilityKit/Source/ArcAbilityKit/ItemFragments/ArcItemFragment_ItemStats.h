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
#include "Items/ArcItemInstance.h"
#include "Attributes/ArcAttribute.h"
#include "ArcScalableFloat.h"
#include "GameplayTagContainer.h"
#include "Misc/Optional.h"

#include "ArcItemFragment_ItemStats.generated.h"

struct FArcItemData;

UENUM(BlueprintType)
enum class EArcModType : uint8
{
	/* Set Mod to Additive and just pass value. */
	Additive = 0,
	/* Set Mod to Multiplicative. */
	Multiply = 1,
	Division = 2,
};

/** Design-time entry on the fragment — one stat modifier keyed by FArcAttributeRef. */
USTRUCT(BlueprintType)
struct ARCABILITYKIT_API FArcItemAttributeStat
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, SaveGame, Category = "Data")
	EArcModType Type = EArcModType::Additive;

	UPROPERTY(EditAnywhere, SaveGame, Category = "Data")
	FArcScalableFloat Value;

	UPROPERTY(EditAnywhere, SaveGame, Category = "Data")
	FArcAttributeRef Stat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Data")
	FGameplayTagContainer StatTags;

	FArcItemAttributeStat() = default;

	FArcItemAttributeStat(const FArcAttributeRef& InStat, float InValue)
		: Value(InValue)
		, Stat(InStat)
	{
	}

	FArcItemAttributeStat& SetValue(float V) { Value = V; return *this; }

	bool operator==(const FArcItemAttributeStat& Other) const { return Stat == Other.Stat && StatTags == Other.StatTags; }
	bool IsValid() const { return Stat.IsValid(); }
};

/** Replicated aggregate: one value per modifier kind per (Stat, StatTags) key; unset kinds hold the neutral value. */
USTRUCT()
struct ARCABILITYKIT_API FArcItemStatReplicated
{
	GENERATED_BODY()

	UPROPERTY()
	FArcAttributeRef Stat;

	UPROPERTY()
	FGameplayTagContainer StatTags;

	UPROPERTY()
	float Additive = 0.f;

	UPROPERTY()
	float Multiply = 1.f;

	UPROPERTY()
	float Division = 1.f;

	bool operator==(const FArcItemStatReplicated& Other) const
	{
		return Stat == Other.Stat && StatTags == Other.StatTags && Additive == Other.Additive && Multiply == Other.Multiply
			&& Division == Other.Division;
	}
};

/** Mutable instance data holding aggregated item stats, one replicated entry per (Stat, StatTags) key. */
USTRUCT(meta = (ToolTip = "Mutable instance data holding aggregated item stats, one replicated entry per (Stat, StatTags) key."))
struct ARCABILITYKIT_API FArcItemInstance_ItemStats : public FArcItemInstance_ItemData
{
	GENERATED_BODY()

	friend struct FArcItemFragment_ItemStats;

protected:
	// Entries with equal Stat and StatTags are merged in Recalculate, so each (Stat, StatTags) key appears once.
	UPROPERTY(SaveGame)
	TArray<FArcItemStatReplicated> ReplicatedItemStats;

public:
	const TArray<FArcItemStatReplicated>& GetStatEntries() const { return ReplicatedItemStats; }

	float GetStatValue(const FArcAttributeRef& InStat, const FGameplayTagContainer& InTags = FGameplayTagContainer::EmptyContainer) const;

	virtual ~FArcItemInstance_ItemStats() override = default;

	virtual bool Equals(const FArcItemInstance& Other) const override
	{
		const FArcItemInstance_ItemStats& O = static_cast<const FArcItemInstance_ItemStats&>(Other);
		return ReplicatedItemStats == O.ReplicatedItemStats;
	}

	virtual bool ShouldPersist() const override { return true; }
};

/** Item-intrinsic numeric stats (damage, durability, fire rate, etc.). Aggregated from this
 *  item's DefaultStats plus stats on socketed children. Stats are keyed by FArcAttributeRef plus
 *  StatTags; the FArcAttributeRef is the same symbol used for entity-side Mass attributes, so
 *  executions can join item-side and entity-side values. Creates FArcItemInstance_ItemStats.
 */
USTRUCT(meta = (DisplayName = "Item Stats", Category = "Stats", ToolTip = "Item-intrinsic numeric stats (damage, durability, fire rate, etc.). Aggregated from this item's DefaultStats plus stats on socketed children. Stats are keyed by FArcAttributeRef plus StatTags; the FArcAttributeRef is the same symbol used for entity-side Mass attributes, so executions can join item-side and entity-side values. Creates FArcItemInstance_ItemStats."))
struct ARCABILITYKIT_API FArcItemFragment_ItemStats : public FArcItemFragment_ItemInstanceBase
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame, EditAnywhere, Category = "Data")
	TArray<FArcItemAttributeStat> DefaultStats;

	virtual ~FArcItemFragment_ItemStats() override = default;

	virtual UScriptStruct* GetItemInstanceType() const override
	{
		return FArcItemInstance_ItemStats::StaticStruct();
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcItemFragment_ItemStats::StaticStruct();
	}

	virtual void OnItemInitialize(const FArcItemData* InItem) const override;
	virtual void OnItemChanged(const FArcItemData* InItem) const override;

private:
	static void Recalculate(FArcItemInstance_ItemStats& Instance, const FArcItemData* Item);
};

namespace ArcItemStats
{
	ARCABILITYKIT_API void BuildEntries(TConstArrayView<const FArcItemFragment_ItemStats*> Fragments, TArray<FArcItemStatReplicated>& OutEntries);

	ARCABILITYKIT_API TOptional<float> ReadStat(TConstArrayView<FArcItemStatReplicated> Entries, const FArcAttributeRef& Stat,
	                                            const FGameplayTagContainer& Tags);
}
