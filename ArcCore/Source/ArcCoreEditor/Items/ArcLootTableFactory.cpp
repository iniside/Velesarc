// Copyright Lukasz Baran. All Rights Reserved.

#include "Items/ArcLootTableFactory.h"
#include "Items/Loot/ArcLootTable.h"

UArcLootTableFactory::UArcLootTableFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcLootTable::StaticClass();
}

UObject* UArcLootTableFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcLootTable>(InParent, Class, Name, Flags);
}
