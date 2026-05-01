// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/Entity/ArcAreaPopulationDefinitionFactory.h"
#include "PopulationSpawner/ArcAreaPopulationTypes.h"

UArcAreaPopulationDefinitionFactory::UArcAreaPopulationDefinitionFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcAreaPopulationDefinition::StaticClass();
}

UObject* UArcAreaPopulationDefinitionFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcAreaPopulationDefinition>(InParent, Class, Name, Flags);
}
