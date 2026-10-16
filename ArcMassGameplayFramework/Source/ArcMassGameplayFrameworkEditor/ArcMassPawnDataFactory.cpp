// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassPawnDataFactory.h"
#include "Pawn/ArcMassPawnData.h"

UArcMassPawnDataFactory::UArcMassPawnDataFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcMassPawnData::StaticClass();
}

UObject* UArcMassPawnDataFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcMassPawnData>(InParent, Class, Name, Flags);
}
