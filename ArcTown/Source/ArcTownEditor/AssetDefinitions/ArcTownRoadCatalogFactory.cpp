// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/ArcTownRoadCatalogFactory.h"
#include "Catalog/ArcTownRoadCatalog.h"

UArcTownRoadCatalogFactory::UArcTownRoadCatalogFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcTownRoadCatalog::StaticClass();
}

UObject* UArcTownRoadCatalogFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcTownRoadCatalog>(InParent, Class, Name, Flags);
}
