// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassEquipmentSlotPresetFactory.h"
#include "Equipment/ArcMassEquipmentSlotPreset.h"

UArcMassEquipmentSlotPresetFactory::UArcMassEquipmentSlotPresetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcMassEquipmentSlotPreset::StaticClass();
}

UObject* UArcMassEquipmentSlotPresetFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcMassEquipmentSlotPreset>(InParent, Class, Name, Flags);
}
