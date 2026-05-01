// Copyright Lukasz Baran. All Rights Reserved.

#include "Experience/ArcUserFacingExperienceDefinitionFactory.h"
#include "GameMode/ArcUserFacingExperienceDefinition.h"

UArcUserFacingExperienceDefinitionFactory::UArcUserFacingExperienceDefinitionFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcUserFacingExperienceDefinition::StaticClass();
}

UObject* UArcUserFacingExperienceDefinitionFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcUserFacingExperienceDefinition>(InParent, Class, Name, Flags);
}
