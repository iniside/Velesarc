// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassExperienceActionSetFactory.h"
#include "Experience/ArcMassExperienceActionSet.h"

UArcMassExperienceActionSetFactory::UArcMassExperienceActionSetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcMassExperienceActionSet::StaticClass();
}

UObject* UArcMassExperienceActionSetFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcMassExperienceActionSet>(InParent, Class, Name, Flags);
}
