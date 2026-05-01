// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcAreaTypes.h"
#include "ArcTQSTypes.h"

namespace ArcTQS::Area
{
	namespace Names
	{
		inline const FName AreaHandle = FName("AreaHandle");
		inline const FName SlotHandle = FName("SlotHandle");
	}

	/** Build a template property bag with the area schema (AreaHandle only). */
	inline FInstancedPropertyBag MakeAreaTemplate()
	{
		FInstancedPropertyBag Bag;
		Bag.AddProperty(Names::AreaHandle, EPropertyBagPropertyType::Struct, FArcAreaHandle::StaticStruct());
		return Bag;
	}

	/** Build a template property bag with the area slot schema (SlotHandle). */
	inline FInstancedPropertyBag MakeSlotTemplate()
	{
		FInstancedPropertyBag Bag;
		Bag.AddProperty(Names::SlotHandle, EPropertyBagPropertyType::Struct, FArcAreaSlotHandle::StaticStruct());
		return Bag;
	}

	/** Write area handle into item's property bag (bag must already have schema). */
	inline void SetAreaHandle(FInstancedPropertyBag& Bag, FArcAreaHandle Handle)
	{
		Bag.SetValueStruct(Names::AreaHandle, Handle);
	}

	/** Write slot handle into item's property bag (bag must already have schema). */
	inline void SetSlotHandle(FInstancedPropertyBag& Bag, const FArcAreaSlotHandle& Handle)
	{
		Bag.SetValueStruct(Names::SlotHandle, Handle);
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

	/** Read slot handle from item's property bag. */
	inline FArcAreaSlotHandle GetSlotHandle(const FArcTQSTargetItem& Item)
	{
		const auto Result = Item.ItemData.GetValueStruct<FArcAreaSlotHandle>(Names::SlotHandle);
		if (Result.IsValid())
		{
			return *Result.GetValue();
		}
		UE_LOG(LogTemp, Warning, TEXT("ArcTQS::Area::GetSlotHandle — ItemData does not contain '%s'"), *Names::SlotHandle.ToString());
		return FArcAreaSlotHandle();
	}
}
