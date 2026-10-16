// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcEcotopeModelFactory.generated.h"

UCLASS()
class UArcEcotopeModelFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcEcotopeModelFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
