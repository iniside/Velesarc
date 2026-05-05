#include "Fragments/ArcItemFragment_GrantedMassAbilities.h"
#include "Items/ArcItemsHelpers.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "MassAbilities/ArcAbilitySourceData_Item.h"
#include "Engine/World.h"
#include "MassEntitySubsystem.h"
#include "MassEntityManager.h"

void FArcItemFragment_GrantedMassAbilities::OnItemAddedToSlot(const FArcItemData* InItem,
                                                               const FGameplayTag& InSlotId) const
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

	FArcItemInstance_GrantedMassAbilities* Instance =
		ArcItemsHelper::FindMutableInstance<FArcItemInstance_GrantedMassAbilities>(InItem);

	for (const FArcMassAbilityEntry& Entry : Abilities)
	{
		if (!Entry.AbilityDefinition)
		{
			continue;
		}

		FInstancedStruct SourceData;
		SourceData.InitializeAs<FArcAbilitySourceData_Item>();
		FArcAbilitySourceData_Item& ItemSourceData = SourceData.GetMutable<FArcAbilitySourceData_Item>();
		ItemSourceData.ItemDefinition = InItem->GetItemDefinition();
		ItemSourceData.ItemId = InItem->GetItemId();
		ItemSourceData.ItemDataHandle = FArcItemDataHandle(const_cast<FArcItemData*>(InItem));
		ItemSourceData.SlotId = InSlotId;

		TOptional<FArcAbilityHandle> Handle = ArcAbilities::TryGrantAbilitySafe(
			EntityManager, Entity, Entry.AbilityDefinition, Entry.InputTag,
			Entity, SourceData);

		if (Handle.IsSet())
		{
			Instance->GrantedAbilities.Add(Handle.GetValue());
		}
	}
}

void FArcItemFragment_GrantedMassAbilities::OnItemRemovedFromSlot(const FArcItemData* InItem,
                                                                    const FGameplayTag& InSlotId) const
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

	FArcItemInstance_GrantedMassAbilities* Instance =
		ArcItemsHelper::FindMutableInstance<FArcItemInstance_GrantedMassAbilities>(InItem);

	for (const FArcAbilityHandle& Handle : Instance->GrantedAbilities)
	{
		ArcAbilities::TryRemoveAbilitySafe(EntityManager, Entity, Handle);
	}
	Instance->GrantedAbilities.Empty();
}

#if WITH_EDITOR
FArcFragmentDescription FArcItemFragment_GrantedMassAbilities::GetDescription(const UScriptStruct* InStruct) const
{
	FArcFragmentDescription Desc = FArcItemFragment_ItemInstanceBase::GetDescription(InStruct);
	Desc.Prerequisites = { FName(TEXT("UArcCoreMassAgentComponent")) };
	Desc.UsageNotes = TEXT(
		"Grants Mass abilities when the item is equipped to a slot. "
		"Abilities are revoked on unequip. Requires the owning actor to have a UArcCoreMassAgentComponent "
		"with a valid Mass entity that has FArcAbilityCollectionFragment.");
	return Desc;
}
#endif
