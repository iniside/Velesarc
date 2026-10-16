#include "ItemFragments/ArcItemFragment_MassAbilityEffects.h"

#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"
#include "ArcLogs.h"
#include "Engine/World.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityManager.h"

TArray<const FArcMassEffectSpecItem*> FArcItemInstance_MassAbilityEffects::GetSpecsByTag(const FGameplayTag& InTag) const
{
	TArray<const FArcMassEffectSpecItem*> Out;
	for (const FArcMassEffectSpecItem& Spec : CachedSpecs)
	{
		if (Spec.EffectTag.MatchesTag(InTag))
		{
			Out.Add(&Spec);
		}
	}
	return Out;
}

namespace ArcMassAbilityEffectsInternal
{
	void AddSpecsFromFragment(
		const FArcItemFragment_MassAbilityEffects* InFragment,
		const FArcItemId& InItemId,
		FArcItemInstance_MassAbilityEffects* Instance)
	{
		for (const TPair<FGameplayTag, FArcMassEffectEntry>& Pair : InFragment->Effects)
		{
			FArcMassEffectSpecItem SpecItem;
			SpecItem.EffectTag = Pair.Key;
			SpecItem.SourceItemId = InItemId;
			SpecItem.SourceRequiredTags = Pair.Value.SourceRequiredTags;
			SpecItem.SourceIgnoreTags = Pair.Value.SourceIgnoreTags;
			SpecItem.TargetRequiredTags = Pair.Value.TargetRequiredTags;
			SpecItem.TargetIgnoreTags = Pair.Value.TargetIgnoreTags;
			SpecItem.Effects = Pair.Value.Effects;
			Instance->CachedSpecs.Add(MoveTemp(SpecItem));
		}
	}

	bool HasSpecsForItem(const FArcItemInstance_MassAbilityEffects* Instance, const FArcItemId& InItemId)
	{
		int32 Idx = Instance->CachedSpecs.IndexOfByPredicate(
			[&InItemId](const FArcMassEffectSpecItem& Item) { return Item.SourceItemId == InItemId; });
		return Idx != INDEX_NONE;
	}
}

void FArcItemFragment_MassAbilityEffects::OnItemInitialize(const FArcItemData* InItem) const
{
	if (!InItem->MassEntityHandle.IsValid() || !InItem->World.IsValid())
		return;
	UMassEntitySubsystem* Subsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(InItem->World.Get());
	if (!Subsystem)
		return;
	FMassEntityManager& EntityManager = Subsystem->GetMutableEntityManager();
	const FMassEntityHandle Entity = InItem->MassEntityHandle;
	if (!EntityManager.IsEntityValid(Entity))
		return;

	FArcItemInstance_MassAbilityEffects* Instance =
		ArcItemsHelper::FindMutableInstance<FArcItemInstance_MassAbilityEffects>(InItem);
	if (Instance == nullptr)
	{
		return;
	}

	if (ArcMassAbilityEffectsInternal::HasSpecsForItem(Instance, InItem->GetItemId()))
	{
		UE_LOGF(LogArcCore
			, Log
			,"%ls Mass effect spec already initialized.", *InItem->GetItemDefinitionId().ToString());
		return;
	}

	ArcMassAbilityEffectsInternal::AddSpecsFromFragment(this, InItem->GetItemId(), Instance);
}

void FArcItemFragment_MassAbilityEffects::OnItemChanged(const FArcItemData* InItem) const
{
	// Actor-side reconciliation retired with UArcItemsStoreComponent. In Mass-native
	// context, effect spec caching is handled at OnItemInitialize time:
	//   1. The Effects fragment data is immutable at runtime
	//   2. Mass ops (AddItem/RemoveItem/Attach/Detach) manage lifecycle via
	//      Mass-specific paths, not through FArcItemData::OnItemChanged
	//   3. Tag-based filtering of specs happens at runtime in the ability task
	if (!InItem->MassEntityHandle.IsValid() || !InItem->World.IsValid())
		return;
	UMassEntitySubsystem* Subsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(InItem->World.Get());
	if (!Subsystem)
		return;
	FMassEntityManager& EntityManager = Subsystem->GetMutableEntityManager();
	if (!EntityManager.IsEntityValid(InItem->MassEntityHandle))
		return;
}

#if WITH_EDITOR
FArcFragmentDescription FArcItemFragment_MassAbilityEffects::GetDescription(const UScriptStruct* InStruct) const
{
	FArcFragmentDescription Desc = FArcItemFragment_ItemInstanceBase::GetDescription(InStruct);
	Desc.CommonPairings = {
		FName(TEXT("FArcItemFragment_GrantedMassAbilities")),
		FName(TEXT("FArcItemFragment_GrantedMassEffects"))
	};
	Desc.Prerequisites = { FName(TEXT("UArcCoreMassAgentComponent")) };
	Desc.UsageNotes = TEXT(
		"Pre-caches Mass effect definitions at item initialization, keyed by gameplay tag. "
		"Abilities query cached specs at runtime to apply effects on hit or use.");
	return Desc;
}
#endif
