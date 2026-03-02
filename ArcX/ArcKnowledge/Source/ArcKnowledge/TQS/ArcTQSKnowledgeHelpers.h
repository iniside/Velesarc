// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcKnowledgeTypes.h"
#include "TargetQuery/ArcTQSTypes.h"

namespace ArcTQS::Knowledge
{
	// Property names used in the knowledge item data property bag.
	namespace Names
	{
		inline const FName Handle = FName("KnowledgeHandle");
	}

	/** Write the knowledge handle into the item's property bag. */
	inline void SetItemData(FArcTQSTargetItem& Item, FArcKnowledgeHandle Handle)
	{
		Item.ItemData.AddProperty(Names::Handle, EPropertyBagPropertyType::Struct, FArcKnowledgeHandle::StaticStruct());
		Item.ItemData.SetValueStruct(Names::Handle, Handle);
	}

	/** Read the knowledge handle from the item's property bag. Returns invalid handle on type mismatch. */
	inline FArcKnowledgeHandle GetHandle(const FArcTQSTargetItem& Item)
	{
		const auto Result = Item.ItemData.GetValueStruct<FArcKnowledgeHandle>(Names::Handle);
		if (Result.IsValid())
		{
			return *Result.GetValue();
		}
		UE_LOG(LogTemp, Warning, TEXT("ArcTQS::Knowledge::GetHandle — ItemData does not contain '%s'"), *Names::Handle.ToString());
		return FArcKnowledgeHandle();
	}
}
