// Copyright Lukasz Baran. All Rights Reserved.

#include "Items/ArcItemSocketSlotsPresetFactory.h"
#include "Items/Fragments/ArcItemFragment_SocketSlots.h"

UArcItemSocketSlotsPresetFactory::UArcItemSocketSlotsPresetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcItemSocketSlotsPreset::StaticClass();
}

UObject* UArcItemSocketSlotsPresetFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcItemSocketSlotsPreset>(InParent, Class, Name, Flags);
}
