// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/ArcAbilityDefinitionFactory.h"
#include "Abilities/ArcAbilityDefinition.h"

UArcAbilityDefinitionFactory::UArcAbilityDefinitionFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcAbilityDefinition::StaticClass();
}

UObject* UArcAbilityDefinitionFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcAbilityDefinition>(InParent, Class, Name, Flags);
}
