#include "Fragments/ArcItemFragment_MassAbilityEffects.h"

#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"
#include "ArcLogs.h"
#include "Engine/World.h"
#include "MassEntitySubsystem.h"
#include "MassEntityManager.h"

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
		UE_LOG(LogArcCore
			, Log
			, TEXT("%s Mass effect spec already initialized.")
			, *InItem->GetItemDefinition()->GetName());
		return;
	}

	ArcMassAbilityEffectsInternal::AddSpecsFromFragment(this, InItem->GetItemId(), Instance);
}

void FArcItemFragment_MassAbilityEffects::OnItemChanged(const FArcItemData* InItem) const
{
	if (!InItem->OwnerComponent)
	{
		// Mass context: effect spec caching is handled at OnItemInitialize time.
		// Reconciliation of CachedSpecs isn't needed here because:
		//   1. The Effects fragment data is immutable at runtime
		//   2. Mass ops (AddItem/RemoveItem/Attach/Detach) manage lifecycle via
		//      Mass-specific paths, not through FArcItemData::OnItemChanged
		//   3. Tag-based filtering of specs happens at runtime in the ability task
		// For safety, validate the Mass entity exists before returning.
		if (!InItem->MassEntityHandle.IsValid() || !InItem->World.IsValid())
			return;
		UMassEntitySubsystem* Subsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(InItem->World.Get());
		if (!Subsystem)
			return;
		FMassEntityManager& EntityManager = Subsystem->GetMutableEntityManager();
		if (!EntityManager.IsEntityValid(InItem->MassEntityHandle))
			return;
		return;
	}

	const TArray<const FArcItemData*>& ItemsInSockets = InItem->GetItemsInSockets();
	const FArcItemData* OwnerItemPtr = InItem->GetItemsStoreComponent()->GetItemPtr(InItem->GetOwnerId());

	TArray<FArcItemId> ExistingItems;
	ExistingItems.Add(InItem->GetItemId());

	for (const FArcItemData* SocketItem : ItemsInSockets)
	{
		ExistingItems.Add(SocketItem->GetItemId());
	}

	FArcItemInstance_MassAbilityEffects* Instance = nullptr;
	if (OwnerItemPtr)
	{
		Instance = ArcItemsHelper::FindMutableInstance<FArcItemInstance_MassAbilityEffects>(OwnerItemPtr);
	}
	else
	{
		Instance = ArcItemsHelper::FindMutableInstance<FArcItemInstance_MassAbilityEffects>(InItem);
	}

	if (Instance == nullptr)
	{
		if (InItem->GetItemsStoreComponent()->GetOwnerRole() == ENetRole::ROLE_Authority)
		{
			ArcItemsHelper::AddInstance<FArcItemInstance_MassAbilityEffects>(OwnerItemPtr);
			Instance = ArcItemsHelper::FindMutableInstance<FArcItemInstance_MassAbilityEffects>(OwnerItemPtr);
			if (Instance == nullptr)
			{
				return;
			}
		}
		if (InItem->GetItemsStoreComponent()->GetOwnerRole() < ENetRole::ROLE_Authority)
		{
			return;
		}
	}

	// Remove specs for items no longer present (reverse iterate for safe removal)
	for (int32 Idx = Instance->CachedSpecs.Num() - 1; Idx >= 0; --Idx)
	{
		bool bFound = false;
		for (const FArcItemId& Id : ExistingItems)
		{
			if (Instance->CachedSpecs[Idx].SourceItemId == Id)
			{
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			Instance->CachedSpecs.RemoveAt(Idx);
		}
	}

	// Add specs for the main item if not already present
	if (!ArcMassAbilityEffectsInternal::HasSpecsForItem(Instance, InItem->GetItemId()))
	{
		ArcMassAbilityEffectsInternal::AddSpecsFromFragment(this, InItem->GetItemId(), Instance);
	}

	// Add specs for socket items if not already present
	for (const FArcItemData* SocketItem : ItemsInSockets)
	{
		if (!ArcMassAbilityEffectsInternal::HasSpecsForItem(Instance, SocketItem->GetItemId()))
		{
			ArcMassAbilityEffectsInternal::AddSpecsFromFragment(this, SocketItem->GetItemId(), Instance);
		}
	}
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
