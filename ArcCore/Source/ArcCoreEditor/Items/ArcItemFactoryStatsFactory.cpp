// Copyright Lukasz Baran. All Rights Reserved.

#include "Items/ArcItemFactoryStatsFactory.h"
#include "Items/Factory/ArcItemFactoryStats.h"

UArcItemFactoryStatsFactory::UArcItemFactoryStatsFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcItemFactoryStats::StaticClass();
}

UObject* UArcItemFactoryStatsFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcItemFactoryStats>(InParent, Class, Name, Flags);
}
