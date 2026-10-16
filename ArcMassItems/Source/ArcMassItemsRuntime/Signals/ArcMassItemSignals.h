// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"

namespace UE::ArcMassItems::Signals
{
	// Generic (un-qualified) signal names. Subscribers receive signals from any store type;
	// filter by FArcMassItemEvent::StoreType inside the processor.
	inline const FName ItemAdded         = FName(TEXT("ArcMassItemAdded"));
	inline const FName ItemRemoved       = FName(TEXT("ArcMassItemRemoved"));
	inline const FName ItemSlotAssigned  = FName(TEXT("ArcMassItemSlotAssigned"));
	inline const FName ItemSlotCleared   = FName(TEXT("ArcMassItemSlotCleared"));
	inline const FName ItemSlotChanged   = FName(TEXT("ArcMassItemSlotChanged")); // move: slot A → slot B
	inline const FName ItemStacksChanged = FName(TEXT("ArcMassItemStacksChanged"));
	inline const FName ItemAttached      = FName(TEXT("ArcMassItemAttached"));
	inline const FName ItemDetached      = FName(TEXT("ArcMassItemDetached"));
	inline const FName ItemAttachmentChanged = FName(TEXT("ArcMassItemAttachmentChanged"));
	inline const FName ItemEquipped          = FName(TEXT("ArcMassItemEquipped"));
	inline const FName ItemUnequipped        = FName(TEXT("ArcMassItemUnequipped"));
}
