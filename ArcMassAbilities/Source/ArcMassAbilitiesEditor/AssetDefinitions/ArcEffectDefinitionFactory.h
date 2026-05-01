// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcEffectDefinitionFactory.generated.h"

UCLASS()
class UArcEffectDefinitionFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcEffectDefinitionFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
