// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcBuildPostPlaceAction.h"

#include "ItemFragments/ArcItemFragment_ItemStats.h"
#include "Items/ArcItemDefinitionData.h"
#include "Mass/EntityManager.h"
#include "UObject/UnrealType.h"

namespace ArcBuildPostPlaceActionInternal
{
	static bool EvaluateStatOnDefinition(const FArcItemDefinitionData* ItemDef, const FArcAttributeRef& Stat, float& OutValue)
	{
		if (ItemDef == nullptr)
		{
			return false;
		}

		const FArcItemFragment_ItemStats* StatsFragment = ItemDef->FindFragment<FArcItemFragment_ItemStats>();
		if (StatsFragment == nullptr)
		{
			return false;
		}

		TArray<FArcItemStatReplicated> Entries;
		ArcItemStats::BuildEntries(TConstArrayView<const FArcItemFragment_ItemStats*>(&StatsFragment, 1), Entries);
		const TOptional<float> Value = ArcItemStats::ReadStat(Entries, Stat, FGameplayTagContainer::EmptyContainer);
		if (!Value.IsSet())
		{
			return false;
		}

		OutValue = Value.GetValue();
		return true;
	}

	/**
	 * Aggregates one mapping's source stat over all consumed item specs.
	 * Sum/Average weight each item's value by the stacks consumed (FArcItemSpec::Amount);
	 * Max ignores stack counts. Scale applies last.
	 * @return false if no consumed item carried the source stat.
	 */
	static bool ComputeMappingValue(const FArcBuildStatMapping& Mapping, TConstArrayView<FArcItemSpec> ConsumedItems, float& OutValue)
	{
		float Sum = 0.f;
		float Max = -FLT_MAX;
		int32 TotalStacks = 0;
		bool bAnyMatch = false;

		for (const FArcItemSpec& Spec : ConsumedItems)
		{
			float ItemValue = 0.f;
			if (!EvaluateStatOnDefinition(Spec.GetItemDefinition(), Mapping.SourceItemStat, ItemValue))
			{
				continue;
			}
			bAnyMatch = true;

			const int32 Stacks = FMath::Max(1, static_cast<int32>(Spec.Amount));
			Sum += ItemValue * static_cast<float>(Stacks);
			TotalStacks += Stacks;
			Max = FMath::Max(Max, ItemValue);
		}

		if (!bAnyMatch)
		{
			return false;
		}

		float Aggregated = 0.f;
		switch (Mapping.Agg)
		{
			case EArcBuildStatAgg::Sum:
				Aggregated = Sum;
				break;
			case EArcBuildStatAgg::Average:
				Aggregated = (TotalStacks > 0) ? (Sum / static_cast<float>(TotalStacks)) : 0.f;
				break;
			case EArcBuildStatAgg::Max:
				Aggregated = Max;
				break;
			default:
				break;
		}

		OutValue = Aggregated * Mapping.Scale;
		return true;
	}

	/**
	 * Reflection-writes Value into the FArcAttribute member named by AttributeRef inside
	 * FragmentMemory (an instance of AttributeRef.FragmentType), seeding BaseValue AND
	 * CurrentValue via InitValue. Shared by the spawn-override and PostPlaced phases.
	 * @return false if the property is missing or is not an FArcAttribute.
	 */
	static bool WriteAttribute(const FArcAttributeRef& AttributeRef, uint8* FragmentMemory, float Value)
	{
		if (FragmentMemory == nullptr)
		{
			return false;
		}

		FStructProperty* StructProp = CastField<FStructProperty>(AttributeRef.GetCachedProperty());
		if (StructProp == nullptr || StructProp->Struct != FArcAttribute::StaticStruct())
		{
			return false;
		}

		FArcAttribute* Attribute = StructProp->ContainerPtrToValuePtr<FArcAttribute>(FragmentMemory);
		if (Attribute == nullptr)
		{
			return false;
		}

		Attribute->InitValue(Value);
		return true;
	}

	/** Validates the mapping target: both refs set, target fragment derives from FArcMassAttributesBase. */
	static bool IsMappingValid(const FArcBuildStatMapping& Mapping)
	{
		if (!Mapping.SourceItemStat.IsValid() || !Mapping.TargetAttribute.IsValid())
		{
			return false;
		}
		if (!Mapping.TargetAttribute.FragmentType->IsChildOf(FArcMassAttributesBase::StaticStruct()))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("FArcBuildPostPlaceAction_IngredientStats: target fragment '%s' does not derive from FArcMassAttributesBase — mapping skipped."),
				*Mapping.TargetAttribute.FragmentType->GetName());
			return false;
		}
		return true;
	}
} // namespace ArcBuildPostPlaceActionInternal

void FArcBuildPostPlaceAction_IngredientStats::BuildSpawnOverrides(
	const FArcBuildPlacementContext& Ctx,
	TArray<FInstancedStruct>& InOutOverrides) const
{
	for (const FArcBuildStatMapping& Mapping : Mappings)
	{
		if (!ArcBuildPostPlaceActionInternal::IsMappingValid(Mapping))
		{
			continue;
		}

		float Total = 0.f;
		if (!ArcBuildPostPlaceActionInternal::ComputeMappingValue(Mapping, Ctx.ConsumedItems, Total))
		{
			continue;
		}

		const UScriptStruct* FragmentType = Mapping.TargetAttribute.FragmentType;
		FInstancedStruct* Override = InOutOverrides.FindByPredicate(
			[FragmentType](const FInstancedStruct& Existing)
			{
				return Existing.GetScriptStruct() == FragmentType;
			});
		if (Override == nullptr)
		{
			const int32 NewIndex = InOutOverrides.AddDefaulted();
			Override = &InOutOverrides[NewIndex];
			Override->InitializeAs(FragmentType);
		}

		ArcBuildPostPlaceActionInternal::WriteAttribute(Mapping.TargetAttribute, Override->GetMutableMemory(), Total);
	}
}

void FArcBuildPostPlaceAction_IngredientStats::PostPlaced(
	FMassEntityManager& EntityManager,
	FMassEntityHandle PlacedEntity,
	const FArcBuildPlacementContext& Ctx) const
{
	// Re-apply on the live entity: UArcInitialAttributesObserver re-seeds attribute
	// fragments from its preset during Commit, stomping spawn-override values. This runs
	// after SpawnEntityWithOverrides returned (Commit + creation observers done), so the
	// write here deterministically wins. See the base-struct doc comment.
	for (const FArcBuildStatMapping& Mapping : Mappings)
	{
		if (!ArcBuildPostPlaceActionInternal::IsMappingValid(Mapping))
		{
			continue;
		}

		float Total = 0.f;
		if (!ArcBuildPostPlaceActionInternal::ComputeMappingValue(Mapping, Ctx.ConsumedItems, Total))
		{
			continue;
		}

		const FStructView FragmentView = EntityManager.GetMutableSparseElementDataForEntity(
			Mapping.TargetAttribute.FragmentType, PlacedEntity);
		if (FragmentView.GetMemory() == nullptr)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("FArcBuildPostPlaceAction_IngredientStats: placed entity has no sparse fragment '%s' — PostPlaced re-apply skipped."),
				*Mapping.TargetAttribute.FragmentType->GetName());
			continue;
		}

		ArcBuildPostPlaceActionInternal::WriteAttribute(Mapping.TargetAttribute, FragmentView.GetMemory(), Total);
	}
}
