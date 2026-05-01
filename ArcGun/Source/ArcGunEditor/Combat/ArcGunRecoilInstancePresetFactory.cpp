// Copyright Lukasz Baran. All Rights Reserved.

#include "Combat/ArcGunRecoilInstancePresetFactory.h"
#include "ArcGunRecoilInstance.h"

UArcGunRecoilInstancePresetFactory::UArcGunRecoilInstancePresetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcGunRecoilInstancePreset::StaticClass();
}

UObject* UArcGunRecoilInstancePresetFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcGunRecoilInstancePreset>(InParent, Class, Name, Flags);
}
