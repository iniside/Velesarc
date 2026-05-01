// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/Knowledge/ArcTQSQueryDefinitionFactory.h"
#include "ArcTQSQueryDefinition.h"

UArcTQSQueryDefinitionFactory::UArcTQSQueryDefinitionFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcTQSQueryDefinition::StaticClass();
}

UObject* UArcTQSQueryDefinitionFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcTQSQueryDefinition>(InParent, Class, Name, Flags);
}
