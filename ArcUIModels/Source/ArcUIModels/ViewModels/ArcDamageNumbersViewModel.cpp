// Copyright Lukasz Baran. All Rights Reserved.

#include "ViewModels/ArcDamageNumbersViewModel.h"

void UArcDamageNumbersViewModel::SetEntries(const TArray<FArcDamageNumberEntry>& InEntries)
{
	UE_MVVM_SET_PROPERTY_VALUE(Entries, InEntries);
}
