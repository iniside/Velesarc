// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/World/ArcAreaDefinitionFactory.h"
#include "ArcAreaDefinition.h"

UArcAreaDefinitionFactory::UArcAreaDefinitionFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcAreaDefinition::StaticClass();
}

UObject* UArcAreaDefinitionFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcAreaDefinition>(InParent, Class, Name, Flags);
}
