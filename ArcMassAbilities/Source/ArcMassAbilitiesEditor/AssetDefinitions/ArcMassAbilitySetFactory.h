// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcMassAbilitySetFactory.generated.h"

UCLASS()
class UArcMassAbilitySetFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcMassAbilitySetFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
