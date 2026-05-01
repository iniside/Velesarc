// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/ArcEffectDefinitionFactory.h"
#include "Effects/ArcEffectDefinition.h"

UArcEffectDefinitionFactory::UArcEffectDefinitionFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcEffectDefinition::StaticClass();
}

UObject* UArcEffectDefinitionFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcEffectDefinition>(InParent, Class, Name, Flags);
}
