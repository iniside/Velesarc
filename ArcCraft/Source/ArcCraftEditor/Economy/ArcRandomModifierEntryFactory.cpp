// Copyright Lukasz Baran. All Rights Reserved.

#include "Economy/ArcRandomModifierEntryFactory.h"
#include "ArcCraft/Recipe/ArcRandomModifierEntry.h"

UArcRandomModifierEntryFactory::UArcRandomModifierEntryFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcRandomModifierEntry::StaticClass();
}

UObject* UArcRandomModifierEntryFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcRandomModifierEntry>(InParent, Class, Name, Flags);
}
