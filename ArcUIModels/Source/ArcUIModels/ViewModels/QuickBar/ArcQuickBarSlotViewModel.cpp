// Copyright Lukasz Baran. All Rights Reserved.

#include "ViewModels/QuickBar/ArcQuickBarSlotViewModel.h"

#include "Commands/ArcMassAddItemToQuickSlotCommand.h"
#include "Commands/ArcMassRemoveItemFromQuickSlotCommand.h"
#include "Commands/ArcReplicatedCommandHelpers.h"
#include "ViewModels/ArcItemViewModel.h"

void UArcQuickBarSlotViewModel::SetBarId(FGameplayTag InBarId)
{
	UE_MVVM_SET_PROPERTY_VALUE(BarId, InBarId);
}

void UArcQuickBarSlotViewModel::SetQuickSlotId(FGameplayTag InQuickSlotId)
{
	UE_MVVM_SET_PROPERTY_VALUE(QuickSlotId, InQuickSlotId);
}

void UArcQuickBarSlotViewModel::SetItem(UArcItemViewModel* InItem)
{
	TObjectPtr<UArcItemViewModel> NewItem = InItem;
	UE_MVVM_SET_PROPERTY_VALUE(Item, NewItem);
}

void UArcQuickBarSlotViewModel::SetAbility(UArcAbilityViewModel* InAbility)
{
	TObjectPtr<UArcAbilityViewModel> NewAbility = InAbility;
	UE_MVVM_SET_PROPERTY_VALUE(Ability, NewAbility);
}

void UArcQuickBarSlotViewModel::SetHasItem(bool bInHasItem)
{
	UE_MVVM_SET_PROPERTY_VALUE(bHasItem, bInHasItem);
}

void UArcQuickBarSlotViewModel::SetActive(bool bInActive)
{
	UE_MVVM_SET_PROPERTY_VALUE(bIsActive, bInActive);
}

void UArcQuickBarSlotViewModel::SetEntity(FMassEntityHandle InEntity)
{
	TargetEntity = InEntity;
}

bool UArcQuickBarSlotViewModel::RequestAddItem(UObject* WorldContextObject, UArcItemViewModel* InItem)
{
	if (WorldContextObject == nullptr || InItem == nullptr || !InItem->GetItemId().IsValid() || !TargetEntity.IsSet() || !BarId.IsValid() || !QuickSlotId.IsValid())
	{
		return false;
	}

	return Arcx::SendServerCommand<FArcMassAddItemToQuickSlotCommand>(
		WorldContextObject,
		TargetEntity,
		InItem->GetItemId(),
		BarId,
		QuickSlotId);
}

bool UArcQuickBarSlotViewModel::RequestRemoveItem(UObject* WorldContextObject)
{
	if (WorldContextObject == nullptr || !TargetEntity.IsSet() || !BarId.IsValid() || !QuickSlotId.IsValid())
	{
		return false;
	}

	return Arcx::SendServerCommand<FArcMassRemoveItemFromQuickSlotCommand>(
		WorldContextObject,
		TargetEntity,
		BarId,
		QuickSlotId);
}
