// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcTownRoadCatalogFactory.generated.h"

UCLASS()
class UArcTownRoadCatalogFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcTownRoadCatalogFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
