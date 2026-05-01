// Copyright Lukasz Baran. All Rights Reserved.

#include "Items/ArcEquipmentSlotPresetFactory.h"
#include "Equipment/ArcEquipmentComponent.h"

UArcEquipmentSlotPresetFactory::UArcEquipmentSlotPresetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcEquipmentSlotPreset::StaticClass();
}

UObject* UArcEquipmentSlotPresetFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcEquipmentSlotPreset>(InParent, Class, Name, Flags);
}
