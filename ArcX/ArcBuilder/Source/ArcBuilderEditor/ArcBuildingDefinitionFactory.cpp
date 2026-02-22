// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcBuildingDefinitionFactory.h"
#include "ArcBuildingDefinition.h"

UArcBuildingDefinitionFactory::UArcBuildingDefinitionFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcBuildingDefinition::StaticClass();
}

UObject* UArcBuildingDefinitionFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	UArcBuildingDefinition* NewBuilding = NewObject<UArcBuildingDefinition>(InParent, Class, Name, Flags);
	return NewBuilding;
}
