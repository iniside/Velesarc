// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcEcotopeLowGroundCoverSpeciesModelFactory.generated.h"

UCLASS()
class UArcEcotopeLowGroundCoverSpeciesModelFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcEcotopeLowGroundCoverSpeciesModelFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
