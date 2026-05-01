// Copyright Lukasz Baran. All Rights Reserved.

#include "Combat/ArcTargetingObjectFactory.h"
#include "AbilitySystem/Targeting/ArcTargetingObject.h"

UArcTargetingObjectFactory::UArcTargetingObjectFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcTargetingObject::StaticClass();
}

UObject* UArcTargetingObjectFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcTargetingObject>(InParent, Class, Name, Flags);
}
