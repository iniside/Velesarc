// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcEcotopeTreeSpeciesModelFactory.generated.h"

UCLASS()
class UArcEcotopeTreeSpeciesModelFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcEcotopeTreeSpeciesModelFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
