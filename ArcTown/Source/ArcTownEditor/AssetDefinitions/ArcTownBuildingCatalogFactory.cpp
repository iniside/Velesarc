// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/ArcTownBuildingCatalogFactory.h"
#include "Catalog/ArcTownBuildingCatalog.h"

UArcTownBuildingCatalogFactory::UArcTownBuildingCatalogFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcTownBuildingCatalog::StaticClass();
}

UObject* UArcTownBuildingCatalogFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcTownBuildingCatalog>(InParent, Class, Name, Flags);
}
