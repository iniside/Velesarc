// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcTownBuildingDefinitionFactory.generated.h"

UCLASS()
class UArcTownBuildingDefinitionFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcTownBuildingDefinitionFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
