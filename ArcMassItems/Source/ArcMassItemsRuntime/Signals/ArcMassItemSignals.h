// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"

namespace UE::ArcMassItems::Signals
{
	// Base signal name prefixes. Actual signals are store-type qualified — use GetSignalName.
	inline const FName ItemAdded         = FName(TEXT("ArcMassItemAdded"));
	inline const FName ItemRemoved       = FName(TEXT("ArcMassItemRemoved"));
	inline const FName ItemSlotChanged   = FName(TEXT("ArcMassItemSlotChanged"));
	inline const FName ItemStacksChanged = FName(TEXT("ArcMassItemStacksChanged"));
	inline const FName ItemAttached      = FName(TEXT("ArcMassItemAttached"));
	inline const FName ItemDetached      = FName(TEXT("ArcMassItemDetached"));

	// Returns a store-qualified signal name, e.g. "ArcMassItemAdded.FArcMassItemStoreFragment".
	// Subscribe to the qualified name for the specific store type you care about.
	inline FName GetSignalName(FName BaseSignal, const UScriptStruct* StoreType)
	{
		check(StoreType);
		return FName(*(BaseSignal.ToString() + TEXT(".") + StoreType->GetName()));
	}
}
