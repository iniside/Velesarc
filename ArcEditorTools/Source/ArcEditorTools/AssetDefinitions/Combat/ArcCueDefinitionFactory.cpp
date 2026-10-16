// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/Combat/ArcCueDefinitionFactory.h"
#include "ArcCues/ArcCueDefinition.h"

UArcCueDefinitionFactory::UArcCueDefinitionFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcCueDefinition::StaticClass();
}

UObject* UArcCueDefinitionFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcCueDefinition>(InParent, Class, Name, Flags);
}
