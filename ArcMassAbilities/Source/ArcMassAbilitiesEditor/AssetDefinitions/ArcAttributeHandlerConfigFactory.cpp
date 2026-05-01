// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/ArcAttributeHandlerConfigFactory.h"
#include "Attributes/ArcAttributeHandlerConfig.h"

UArcAttributeHandlerConfigFactory::UArcAttributeHandlerConfigFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcAttributeHandlerConfig::StaticClass();
}

UObject* UArcAttributeHandlerConfigFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcAttributeHandlerConfig>(InParent, Class, Name, Flags);
}
