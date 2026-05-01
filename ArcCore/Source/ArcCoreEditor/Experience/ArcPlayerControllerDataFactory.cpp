// Copyright Lukasz Baran. All Rights Reserved.

#include "Experience/ArcPlayerControllerDataFactory.h"
#include "GameMode/ArcExperienceDefinition.h"

UArcPlayerControllerDataFactory::UArcPlayerControllerDataFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcPlayerControllerData::StaticClass();
}

UObject* UArcPlayerControllerDataFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcPlayerControllerData>(InParent, Class, Name, Flags);
}
