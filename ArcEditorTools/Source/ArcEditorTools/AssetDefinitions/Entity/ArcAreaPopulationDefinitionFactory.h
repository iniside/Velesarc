// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcAreaPopulationDefinitionFactory.generated.h"

UCLASS()
class UArcAreaPopulationDefinitionFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcAreaPopulationDefinitionFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
