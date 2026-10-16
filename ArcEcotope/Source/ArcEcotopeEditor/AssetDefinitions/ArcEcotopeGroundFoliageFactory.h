// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcEcotopeGroundFoliageFactory.generated.h"

UCLASS()
class UArcEcotopeGroundFoliageFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcEcotopeGroundFoliageFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
