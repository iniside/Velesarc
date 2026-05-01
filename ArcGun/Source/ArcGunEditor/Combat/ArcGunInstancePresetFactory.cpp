// Copyright Lukasz Baran. All Rights Reserved.

#include "Combat/ArcGunInstancePresetFactory.h"
#include "ArcGunInstanceBase.h"

UArcGunInstancePresetFactory::UArcGunInstancePresetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcGunInstancePreset::StaticClass();
}

UObject* UArcGunInstancePresetFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcGunInstancePreset>(InParent, Class, Name, Flags);
}
