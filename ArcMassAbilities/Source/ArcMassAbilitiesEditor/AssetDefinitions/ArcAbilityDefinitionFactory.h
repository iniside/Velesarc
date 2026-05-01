// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcAbilityDefinitionFactory.generated.h"

UCLASS()
class UArcAbilityDefinitionFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcAbilityDefinitionFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
