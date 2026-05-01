// Copyright Lukasz Baran. All Rights Reserved.

#include "Combat/ArcGunFireModePresetFactory.h"
#include "ArcGunFireMode.h"

UArcGunFireModePresetFactory::UArcGunFireModePresetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcGunFireModePreset::StaticClass();
}

UObject* UArcGunFireModePresetFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcGunFireModePreset>(InParent, Class, Name, Flags);
}
