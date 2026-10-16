// Copyright Lukasz Baran. All Rights Reserved.

#include "Config/ArcMassEntityConfigTemplateFactory.h"
#include "ArcMass/Config/ArcMassEntityConfigAsset.h"

UArcMassEntityConfigTemplateFactory::UArcMassEntityConfigTemplateFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcMassEntityConfigTemplate::StaticClass();
}

UObject* UArcMassEntityConfigTemplateFactory::FactoryCreateNew(UClass* Class
	, UObject* InParent
	, FName Name
	, EObjectFlags Flags
	, UObject* Context
	, FFeedbackContext* Warn
	, FName CallingContext)
{
	UArcMassEntityConfigTemplate* NewAsset = NewObject<UArcMassEntityConfigTemplate>(InParent, Class, Name, Flags);
	return NewAsset;
}
