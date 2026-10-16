// Copyright Lukasz Baran. All Rights Reserved.

#include "Bags/ArcBagStoreFragment.h"

FArcBagInstanceChangedSignature FArcBagInstanceChanges::OnBagInstanceChanged;

FArcBagInstance* FArcBagReplicatedArray::FindById(FArcBagId BagId)
{
	for (FArcBagInstance& Instance : Items)
	{
		if (Instance.BagId == BagId)
		{
			return &Instance;
		}
	}
	return nullptr;
}

int32 FArcBagReplicatedArray::FindIndexById(FArcBagId BagId) const
{
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		if (Items[Index].BagId == BagId)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}
