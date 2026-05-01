// Copyright Lukasz Baran. All Rights Reserved.

#include "Items/ArcItemFactoryGrantedPassiveAbilitiesFactory.h"
#include "Items/Factory/ArcItemFactoryGrantedPassiveAbilities.h"

UArcItemFactoryGrantedPassiveAbilitiesFactory::UArcItemFactoryGrantedPassiveAbilitiesFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcItemFactoryGrantedPassiveAbilities::StaticClass();
}

UObject* UArcItemFactoryGrantedPassiveAbilitiesFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcItemFactoryGrantedPassiveAbilities>(InParent, Class, Name, Flags);
}
