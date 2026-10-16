// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/ArcTownRoadDefinitionFactory.h"
#include "Catalog/ArcTownRoadDefinition.h"

UArcTownRoadDefinitionFactory::UArcTownRoadDefinitionFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcTownRoadDefinition::StaticClass();
}

UObject* UArcTownRoadDefinitionFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcTownRoadDefinition>(InParent, Class, Name, Flags);
}
