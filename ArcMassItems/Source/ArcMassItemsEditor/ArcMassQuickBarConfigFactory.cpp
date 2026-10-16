// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassQuickBarConfigFactory.h"
#include "QuickBar/ArcMassQuickBarConfig.h"

UArcMassQuickBarConfigFactory::UArcMassQuickBarConfigFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcMassQuickBarConfig::StaticClass();
}

UObject* UArcMassQuickBarConfigFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcMassQuickBarConfig>(InParent, Class, Name, Flags);
}
