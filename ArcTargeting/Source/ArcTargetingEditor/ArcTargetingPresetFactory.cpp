// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTargetingPresetFactory.h"
#include "ArcTargetingPreset.h"

UArcTargetingPresetFactory::UArcTargetingPresetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcTargetingPreset::StaticClass();
}

UObject* UArcTargetingPresetFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcTargetingPreset>(InParent, Class, Name, Flags);
}
