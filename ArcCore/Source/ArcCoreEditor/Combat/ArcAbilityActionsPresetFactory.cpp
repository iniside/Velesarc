// Copyright Lukasz Baran. All Rights Reserved.

#include "Combat/ArcAbilityActionsPresetFactory.h"
#include "AbilitySystem/Fragments/ArcItemFragment_AbilityActions.h"

UArcAbilityActionsPresetFactory::UArcAbilityActionsPresetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcAbilityActionsPreset::StaticClass();
}

UObject* UArcAbilityActionsPresetFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcAbilityActionsPreset>(InParent, Class, Name, Flags);
}
