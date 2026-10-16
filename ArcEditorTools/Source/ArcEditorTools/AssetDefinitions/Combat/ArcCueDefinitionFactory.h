// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"

#include "ArcCueDefinitionFactory.generated.h"

UCLASS()
class UArcCueDefinitionFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcCueDefinitionFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
