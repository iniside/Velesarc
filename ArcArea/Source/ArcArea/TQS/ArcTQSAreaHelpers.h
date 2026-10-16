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
		Bag.AddProperty(Names::AreaHandle, EPropertyBagPropertyType::Struct, FSmartObjectHandle::StaticStruct());
		return Bag;
	}

	/** Build a template property bag with the area slot schema (SlotHandle). */
	inline FInstancedPropertyBag MakeSlotTemplate()
	{
		FInstancedPropertyBag Bag;
		Bag.AddProperty(Names::SlotHandle, EPropertyBagPropertyType::Struct, FSmartObjectSlotHandle::StaticStruct());
		return Bag;
	}

	/** Write area handle into item's property bag (bag must already have schema). */
	inline void SetAreaHandle(FInstancedPropertyBag& Bag, FSmartObjectHandle Handle)
	{
		Bag.SetValueStruct(Names::AreaHandle, Handle);
	}

	/** Write slot handle into item's property bag (bag must already have schema). */
	inline void SetSlotHandle(FInstancedPropertyBag& Bag, const FSmartObjectSlotHandle& Handle)
	{
		Bag.SetValueStruct(Names::SlotHandle, Handle);
	}

	/** Read area handle from item's property bag. */
	inline FSmartObjectHandle GetAreaHandle(const FArcTQSTargetItem& Item)
	{
		const TValueOrError<FSmartObjectHandle*, EPropertyBagResult> Result =
			Item.ItemData.GetValueStruct<FSmartObjectHandle>(Names::AreaHandle);
		if (Result.IsValid())
		{
			return *Result.GetValue();
		}
		UE_LOGF(LogTemp, Warning,"ArcTQS::Area::GetAreaHandle — ItemData does not contain '%ls'", *Names::AreaHandle.ToString());
		return FSmartObjectHandle();
	}

	/** Read slot handle from item's property bag. */
	inline FSmartObjectSlotHandle GetSlotHandle(const FArcTQSTargetItem& Item)
	{
		const TValueOrError<FSmartObjectSlotHandle*, EPropertyBagResult> Result =
			Item.ItemData.GetValueStruct<FSmartObjectSlotHandle>(Names::SlotHandle);
		if (Result.IsValid())
		{
			return *Result.GetValue();
		}
		UE_LOGF(LogTemp, Warning,"ArcTQS::Area::GetSlotHandle — ItemData does not contain '%ls'", *Names::SlotHandle.ToString());
		return FSmartObjectSlotHandle();
	}
}
