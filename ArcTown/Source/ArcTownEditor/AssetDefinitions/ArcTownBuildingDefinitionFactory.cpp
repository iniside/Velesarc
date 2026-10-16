// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/ArcTownBuildingDefinitionFactory.h"
#include "Catalog/ArcTownBuildingDefinition.h"

UArcTownBuildingDefinitionFactory::UArcTownBuildingDefinitionFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcTownBuildingDefinition::StaticClass();
}

UObject* UArcTownBuildingDefinitionFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcTownBuildingDefinition>(InParent, Class, Name, Flags);
}
