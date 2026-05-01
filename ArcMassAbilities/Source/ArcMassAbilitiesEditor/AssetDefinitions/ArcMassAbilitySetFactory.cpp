// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/ArcMassAbilitySetFactory.h"
#include "Abilities/ArcMassAbilitySet.h"

UArcMassAbilitySetFactory::UArcMassAbilitySetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcMassAbilitySet::StaticClass();
}

UObject* UArcMassAbilitySetFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcMassAbilitySet>(InParent, Class, Name, Flags);
}
