// Copyright Lukasz Baran. All Rights Reserved.

#include "World/ArcWorldConditionDefinitionFactory.h"
#include "Conditions/ArcWorldConditionDefinition.h"

UArcWorldConditionDefinitionFactory::UArcWorldConditionDefinitionFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcWorldConditionDefinition::StaticClass();
}

UObject* UArcWorldConditionDefinitionFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcWorldConditionDefinition>(InParent, Class, Name, Flags);
}
