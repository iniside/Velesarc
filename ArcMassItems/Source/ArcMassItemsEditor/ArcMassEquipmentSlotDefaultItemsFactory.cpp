// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassEquipmentSlotDefaultItemsFactory.h"
#include "Equipment/ArcMassEquipmentSlotPreset.h"

UArcMassEquipmentSlotDefaultItemsFactory::UArcMassEquipmentSlotDefaultItemsFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcMassEquipmentSlotDefaultItems::StaticClass();
}

UObject* UArcMassEquipmentSlotDefaultItemsFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcMassEquipmentSlotDefaultItems>(InParent, Class, Name, Flags);
}
