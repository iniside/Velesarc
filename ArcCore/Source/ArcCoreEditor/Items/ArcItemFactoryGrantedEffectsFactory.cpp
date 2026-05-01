// Copyright Lukasz Baran. All Rights Reserved.

#include "Items/ArcItemFactoryGrantedEffectsFactory.h"
#include "Items/Factory/ArcItemFactoryGrantedEffects.h"

UArcItemFactoryGrantedEffectsFactory::UArcItemFactoryGrantedEffectsFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcItemFactoryGrantedEffects::StaticClass();
}

UObject* UArcItemFactoryGrantedEffectsFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcItemFactoryGrantedEffects>(InParent, Class, Name, Flags);
}
