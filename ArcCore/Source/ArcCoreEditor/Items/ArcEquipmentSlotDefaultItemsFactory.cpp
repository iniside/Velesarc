// Copyright Lukasz Baran. All Rights Reserved.

#include "Items/ArcEquipmentSlotDefaultItemsFactory.h"
#include "Equipment/ArcEquipmentComponent.h"

UArcEquipmentSlotDefaultItemsFactory::UArcEquipmentSlotDefaultItemsFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcEquipmentSlotDefaultItems::StaticClass();
}

UObject* UArcEquipmentSlotDefaultItemsFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcEquipmentSlotDefaultItems>(InParent, Class, Name, Flags);
}
