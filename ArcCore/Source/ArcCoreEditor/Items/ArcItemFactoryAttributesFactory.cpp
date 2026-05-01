// Copyright Lukasz Baran. All Rights Reserved.

#include "Items/ArcItemFactoryAttributesFactory.h"
#include "Items/Factory/ArcItemFactoryAttributes.h"

UArcItemFactoryAttributesFactory::UArcItemFactoryAttributesFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcItemFactoryAttributes::StaticClass();
}

UObject* UArcItemFactoryAttributesFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcItemFactoryAttributes>(InParent, Class, Name, Flags);
}
