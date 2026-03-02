// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcAreaTypes.h"
#include "TargetQuery/ArcTQSTypes.h"

namespace ArcTQS::Area
{
	namespace Names
	{
		inline const FName AreaHandle = FName("AreaHandle");
		inline const FName SlotIndex = FName("SlotIndex");
	}

	/** Build a template property bag with the area schema (AreaHandle only). */
	inline FInstancedPropertyBag MakeAreaTemplate()
	{
		FInstancedPropertyBag Bag;
		Bag.AddProperty(Names::AreaHandle, EPropertyBagPropertyType::Struct, FArcAreaHandle::StaticStruct());
		return Bag;
	}

	/** Build a template property bag with the area slot schema (AreaHandle + SlotIndex). */
	inline FInstancedPropertyBag MakeSlotTemplate()
	{
		FInstancedPropertyBag Bag;
		Bag.AddProperties({
			FPropertyBagPropertyDesc(Names::AreaHandle, EPropertyBagPropertyType::Struct, FArcAreaHandle::StaticStruct()),
			FPropertyBagPropertyDesc(Names::SlotIndex, EPropertyBagPropertyType::Int32)
		});
		return Bag;
	}

	/** Write area handle into item's property bag (bag must already have schema). */
	inline void SetAreaHandle(FInstancedPropertyBag& Bag, FArcAreaHandle Handle)
	{
		Bag.SetValueStruct(Names::AreaHandle, Handle);
	}

	/** Write slot index into item's property bag (bag must already have schema). */
	inline void SetSlotIndex(FInstancedPropertyBag& Bag, int32 Index)
	{
		Bag.SetValueInt32(Names::SlotIndex, Index);
	}

	/** Read area handle from item's property bag. */
	inline FArcAreaHandle GetAreaHandle(const FArcTQSTargetItem& Item)
	{
		const auto Result = Item.ItemData.GetValueStruct<FArcAreaHandle>(Names::AreaHandle);
		if (Result.IsValid())
		{
			return *Result.GetValue();
		}
		UE_LOG(LogTemp, Warning, TEXT("ArcTQS::Area::GetAreaHandle — ItemData does not contain '%s'"), *Names::AreaHandle.ToString());
		return FArcAreaHandle();
	}

	/** Read slot index from item's property bag. */
	inline int32 GetSlotIndex(const FArcTQSTargetItem& Item)
	{
		const auto Result = Item.ItemData.GetValueInt32(Names::SlotIndex);
		if (Result.IsValid())
		{
			return Result.GetValue();
		}
		UE_LOG(LogTemp, Warning, TEXT("ArcTQS::Area::GetSlotIndex — ItemData does not contain '%s'"), *Names::SlotIndex.ToString());
		return INDEX_NONE;
	}
}
