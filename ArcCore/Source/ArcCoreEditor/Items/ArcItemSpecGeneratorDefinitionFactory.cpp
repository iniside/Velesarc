// Copyright Lukasz Baran. All Rights Reserved.

#include "Items/ArcItemSpecGeneratorDefinitionFactory.h"
#include "Items/Factory/ArcItemSpecGeneratorDefinition.h"

UArcItemSpecGeneratorDefinitionFactory::UArcItemSpecGeneratorDefinitionFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcItemSpecGeneratorDefinition::StaticClass();
}

UObject* UArcItemSpecGeneratorDefinitionFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcItemSpecGeneratorDefinition>(InParent, Class, Name, Flags);
}
