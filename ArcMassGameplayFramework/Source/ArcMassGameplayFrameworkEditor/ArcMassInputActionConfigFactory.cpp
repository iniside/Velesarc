// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassInputActionConfigFactory.h"
#include "Input/ArcMassInputActionConfig.h"

UArcMassInputActionConfigFactory::UArcMassInputActionConfigFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcMassInputActionConfig::StaticClass();
}

UObject* UArcMassInputActionConfigFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcMassInputActionConfig>(InParent, Class, Name, Flags);
}
