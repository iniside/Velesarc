// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcTownRoadDefinitionFactory.generated.h"

UCLASS()
class UArcTownRoadDefinitionFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcTownRoadDefinitionFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
