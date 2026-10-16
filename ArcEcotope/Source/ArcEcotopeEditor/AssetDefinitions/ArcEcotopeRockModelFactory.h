// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcEcotopeRockModelFactory.generated.h"

UCLASS()
class UArcEcotopeRockModelFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcEcotopeRockModelFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
