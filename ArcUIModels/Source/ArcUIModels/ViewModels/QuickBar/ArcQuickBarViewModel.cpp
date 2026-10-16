// Copyright Lukasz Baran. All Rights Reserved.

#include "ViewModels/QuickBar/ArcQuickBarViewModel.h"

#include "ViewModels/QuickBar/ArcQuickBarSlotViewModel.h"

void UArcQuickBarViewModel::SetBarId(FGameplayTag InBarId)
{
	UE_MVVM_SET_PROPERTY_VALUE(BarId, InBarId);
}

UArcQuickBarSlotViewModel* UArcQuickBarViewModel::FindSlot(FGameplayTag InQuickSlotId) const
{
	for (const TObjectPtr<UArcQuickBarSlotViewModel>& Slot : Slots)
	{
		if (Slot != nullptr && Slot->GetQuickSlotId() == InQuickSlotId)
		{
			return Slot;
		}
	}
	return nullptr;
}
