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


#include "AttributeSet.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemInstance.h"
#include "Items/ArcItemTypes.h"
#include "Items/Fragments/ArcItemFragment.h"

#include "ArcItemFragment_ItemStats.generated.h"

/**
 * Struct which calculates current value of item stat.
 * Replicates only two properties.
 */
USTRUCT()
struct FArcItemStatReplicated
{
	GENERATED_BODY()

public:
	/** GameplayAttribute which defines item stat. */
	UPROPERTY()
	FName Attribute;

	/** Final calculated value of stat. */
	UPROPERTY()
	float FinalValue;

	TArray<FArcStat> Stats;
	float Additive = 0.0f;
	float Multiply = 1.0f;
	float Division = 1.0f;

	FArcItemStatReplicated()
		: Attribute()
		, FinalValue(0)
	{
	};

	bool operator==(const FArcItemStatReplicated& Other) const
	{
		return Attribute == Other.Attribute;
	}

	bool operator==(const FGameplayAttribute& Other) const
	{
		return Attribute == Other.GetUProperty()->GetFName();
	}

	int32 RemoveStat(const FArcItemId& InItemId)
	{
		for (int32 Idx = Stats.Num() - 1; Idx >= 0; Idx--)
		{
			if (Stats[Idx].ItemId == InItemId)
			{
				Stats.RemoveAt(Idx);
			}
		}
		return Stats.Num();
	}
	
	void Recalculate()
	{
		// base values with no tags requirments
		FinalValue = 0.0f;
		Additive = 0.0f;
		Multiply = 1.0f;
		Division = 1.0f;
		for (const FArcStat& ST : Stats)
		{
			switch (ST.Type)
			{
				case EArcModType::Additive:
					Additive += ST.Value;
				break;
				case EArcModType::Multiply:
					Multiply += (ST.Value - 1.0f);
				break;
				case EArcModType::Division:
					Division += (ST.Value - 1.0f);
				break;
				case EArcModType::MAX:
					break;
				default: ;
			}
		}
		// TODO:: Maybe multiply should be 1 by default and when assuming otherwise just
		// subtract 1 ? IDK if (Multiply == 0.0f)
		//{
		//	Multiply = 1.0f;
		//}

		FinalValue = (Additive * Multiply) / Division;
		UE_LOG(LogTemp
			, Log
			, TEXT("Item Recalculate [Value %f] [Additive %f] [Multiply %f]")
			, FinalValue
			, Additive
			, Multiply);
	}
};

USTRUCT()
struct ARCCORE_API FArcItemInstance_ItemStats : public FArcItemInstance_ItemData
{
	GENERATED_BODY()

	friend struct FArcItemFragment_ItemStats;
protected:
	/**
     * Precalculated stats of this item. Only Attribute and FinalValue gets replicated back.
     */
	UPROPERTY()
	TArray<FArcItemStatReplicated> ReplicatedItemStats;

	TMap<FName, float> Stats;
	
public:
	TMap<FName, float>& GetStats()
	{
		return Stats;
	}

	const TMap<FName, float>& GetStats() const
	{
		return Stats;
	}
	
	float GetStatValue(const FName& InAttribute) const
	{
		if (const float* S = Stats.Find(InAttribute))
		{
			return *S;
		}
		return 0.0f;
	}

	float GetStatValue(const FGameplayAttribute& InAttribute) const
	{
		if (const float* S = Stats.Find(InAttribute.GetUProperty()->GetFName()))
		{
			return *S;
		}
		return 0.0f;
	}
	
	const TArray<FArcItemStatReplicated>& GetReplicatedItemStats() const
	{
		return ReplicatedItemStats;
	}
	
	//FArcItemInstance_ItemStats& operator=(const FArcItemInstance_ItemStats& Other)
	//{
	//	const FArcItemInstance_ItemStats* OtherStats = static_cast<const FArcItemInstance_ItemStats*>(&Other);
	//	ReplicatedItemStats = OtherStats->ReplicatedItemStats;
	//	Stats = OtherStats->Stats;
	//	return *this;
	//}
	
	//virtual void OnItemInitialize(const FArcItemData* InItem) override;
	//virtual void OnItemChanged(const FArcItemData* InItem) override;
	
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcItemInstance_ItemStats::StaticStruct();
	}
	
	virtual ~FArcItemInstance_ItemStats() override = default;

	virtual bool Equals(const FArcItemInstance& Other) const override
	{
		const FArcItemInstance_ItemStats& OtherInstance = static_cast<const FArcItemInstance_ItemStats&>(Other);
		if (ReplicatedItemStats.Num() != OtherInstance.ReplicatedItemStats.Num())
		{
			return false;
		}

		
		for (int32 Idx = 0; Idx < ReplicatedItemStats.Num(); Idx++)
		{
			if (ReplicatedItemStats[Idx].FinalValue != OtherInstance.ReplicatedItemStats[Idx].FinalValue)
			{
				return false;
			}
		}

		return true;
	}

	virtual bool ShouldPersist() const override { return true; }
};


/**
 * 
 */
USTRUCT()
struct ARCCORE_API FArcItemFragment_ItemStats : public FArcItemFragment_ItemInstanceBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Data")
	TArray<FArcItemAttributeStat> DefaultStats;

public:
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
	void UpdateStats(const FArcItemData* InItem) const;
	void UpdateStatsInstance(FArcItemInstance_ItemStats* Instance, const FArcItemData* InItem) const;
};