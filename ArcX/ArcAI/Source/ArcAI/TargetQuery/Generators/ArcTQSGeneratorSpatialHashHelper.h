// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "InstancedStruct.h"
#include "ArcMass/ArcMassSpatialHashSubsystem.h"
#include "TargetQuery/ArcTQSTypes.h"

/**
 * Shared utilities for spatial hash generators.
 * Computes index keys from gameplay tags, Mass fragments, and Mass tags,
 * and provides common deduplication logic.
 */
namespace ArcTQSGeneratorSpatialHashHelper
{
	/** Collect individual hash keys from a set of tags, fragments, and mass tags. */
	inline void CollectIndividualKeys(
		const FGameplayTagContainer& Tags,
		const TArray<FInstancedStruct>& Fragments,
		const TArray<FInstancedStruct>& MassTags,
		TArray<uint32>& OutKeys)
	{
		for (const FGameplayTag& Tag : Tags)
		{
			OutKeys.AddUnique(ArcSpatialHash::HashKey(Tag));
		}

		for (const FInstancedStruct& FragmentInstance : Fragments)
		{
			if (const UScriptStruct* StructType = FragmentInstance.GetScriptStruct())
			{
				OutKeys.AddUnique(ArcSpatialHash::HashKey(StructType));
			}
		}

		for (const FInstancedStruct& TagInstance : MassTags)
		{
			if (const UScriptStruct* StructType = TagInstance.GetScriptStruct())
			{
				OutKeys.AddUnique(ArcSpatialHash::HashKey(StructType));
			}
		}
	}

	/** Compute a single combined key from a combination index entry. */
	inline uint32 ComputeCombinationKey(const FArcMassSpatialHashCombinationIndex& Combo)
	{
		TArray<uint32, TInlineAllocator<8>> SubKeys;

		for (const FGameplayTag& Tag : Combo.Tags)
		{
			SubKeys.Add(ArcSpatialHash::HashKey(Tag));
		}
		for (const FInstancedStruct& TagStruct : Combo.MassTags)
		{
			if (const UScriptStruct* ScriptStruct = TagStruct.GetScriptStruct())
			{
				SubKeys.Add(ArcSpatialHash::HashKey(ScriptStruct));
			}
		}
		for (const FInstancedStruct& FragStruct : Combo.Fragments)
		{
			if (const UScriptStruct* ScriptStruct = FragStruct.GetScriptStruct())
			{
				SubKeys.Add(ArcSpatialHash::HashKey(ScriptStruct));
			}
		}

		return ArcSpatialHash::CombineKeys(SubKeys);
	}

	/**
	 * Compute the full set of query keys.
	 *
	 * When bCombineAllIndices is false (default):
	 *   - Each gameplay tag, fragment, and mass tag produces a separate key
	 *   - Each CombinationIndex entry produces a single combined key
	 *   - All keys are queried independently and results merged with dedup
	 *
	 * When bCombineAllIndices is true:
	 *   - All gameplay tags, fragments, and mass tags are combined into one key
	 *   - CombinationIndices are ignored (the single combined key replaces them)
	 *
	 * When no keys are produced, returns empty = query main grid.
	 */
	inline TArray<uint32> ComputeQueryKeys(
		const FGameplayTagContainer& IndexTags,
		const TArray<FInstancedStruct>& IndexFragments,
		const TArray<FInstancedStruct>& IndexMassTags,
		const TArray<FArcMassSpatialHashCombinationIndex>& CombinationIndices,
		bool bCombineAll)
	{
		TArray<uint32> Keys;

		if (bCombineAll)
		{
			// Combine everything into one key
			TArray<uint32> AllSubKeys;
			CollectIndividualKeys(IndexTags, IndexFragments, IndexMassTags, AllSubKeys);
			if (AllSubKeys.Num() > 0)
			{
				Keys.Add(ArcSpatialHash::CombineKeys(AllSubKeys));
			}
			return Keys;
		}

		// Individual keys
		CollectIndividualKeys(IndexTags, IndexFragments, IndexMassTags, Keys);

		// Combination keys
		for (const FArcMassSpatialHashCombinationIndex& Combo : CombinationIndices)
		{
			const uint32 CombinedKey = ComputeCombinationKey(Combo);
			if (CombinedKey != 0)
			{
				Keys.AddUnique(CombinedKey);
			}
		}

		return Keys;
	}

	/**
	 * Add entities from query results to OutItems, deduplicating via SeenEntities.
	 * Stamps ContextIndex on each new item.
	 */
	inline void AddEntities(
		const TArray<FArcMassEntityInfo>& Entities,
		FMassEntityHandle QuerierEntity,
		int32 ContextIdx,
		TSet<FMassEntityHandle>& SeenEntities,
		TArray<FArcTQSTargetItem>& OutItems)
	{
		OutItems.Reserve(OutItems.Num() + Entities.Num());
		for (const FArcMassEntityInfo& Info : Entities)
		{
			if (Info.Entity == QuerierEntity)
			{
				continue;
			}

			bool bAlreadyInSet = false;
			SeenEntities.Add(Info.Entity, &bAlreadyInSet);
			if (bAlreadyInSet)
			{
				continue;
			}

			FArcTQSTargetItem Item;
			Item.TargetType = EArcTQSTargetType::MassEntity;
			Item.EntityHandle = Info.Entity;
			Item.Location = Info.Location;
			Item.ContextIndex = ContextIdx;
			OutItems.Add(MoveTemp(Item));
		}
	}
}
