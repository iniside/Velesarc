// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcTownBuildingCatalogFactory.generated.h"

UCLASS()
class UArcTownBuildingCatalogFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcTownBuildingCatalogFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
