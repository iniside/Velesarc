// Copyright Lukasz Baran. All Rights Reserved.

#include "Items/ArcItemGeneratorDefinitionFactory.h"
#include "Items/Factory/ArcItemSpecGeneratorDefinition.h"

UArcItemGeneratorDefinitionFactory::UArcItemGeneratorDefinitionFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcItemGeneratorDefinition::StaticClass();
}

UObject* UArcItemGeneratorDefinitionFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcItemGeneratorDefinition>(InParent, Class, Name, Flags);
}
