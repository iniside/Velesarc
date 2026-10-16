// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassExperienceDefinitionFactory.h"
#include "Experience/ArcMassExperienceDefinition.h"

UArcMassExperienceDefinitionFactory::UArcMassExperienceDefinitionFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcMassExperienceDefinition::StaticClass();
}

UObject* UArcMassExperienceDefinitionFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcMassExperienceDefinition>(InParent, Class, Name, Flags);
}
