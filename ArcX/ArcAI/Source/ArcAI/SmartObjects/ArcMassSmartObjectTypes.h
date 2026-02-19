// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityHandle.h"
#include "SmartObjectTypes.h"
#include "MassEQSTypes.h"

#include "ArcMassSmartObjectTypes.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct FArcMassSmartObjectItem : public FMassEnvQueryEntityInfo
{
	GENERATED_BODY()
	
public:
	FVector Location;
	FSmartObjectHandle SmartObjectHandle;
	FSmartObjectSlotHandle SlotHandle;
	FMassEntityHandle OwningEntity;
	
	FArcMassSmartObjectItem()
		: Location(FVector::ZeroVector), SmartObjectHandle(FSmartObjectHandle::Invalid), SlotHandle(), OwningEntity()
	{}
	
	FArcMassSmartObjectItem(const FVector& InLocation, const FSmartObjectHandle& InSmartObjectHandle, const FSmartObjectSlotHandle& InSlotHandle)
		: Location(InLocation), SmartObjectHandle(InSmartObjectHandle), SlotHandle(InSlotHandle)
	{}

	FArcMassSmartObjectItem(const FVector& InLocation, const FSmartObjectHandle& InSmartObjectHandle, const FSmartObjectSlotHandle& InSlotHandle, const FMassEntityHandle& InOwningEntity)
		: Location(InLocation), SmartObjectHandle(InSmartObjectHandle), SlotHandle(InSlotHandle), OwningEntity(InOwningEntity)
	{}
	
	inline operator FVector() const { return Location; }

	bool operator==(const FArcMassSmartObjectItem& Other) const
	{
		return SmartObjectHandle == Other.SmartObjectHandle && SlotHandle == Other.SlotHandle && OwningEntity == Other.OwningEntity;
	}
};
