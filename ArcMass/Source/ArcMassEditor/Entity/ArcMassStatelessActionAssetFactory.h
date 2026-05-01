// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcMassStatelessActionAssetFactory.generated.h"

UCLASS()
class UArcMassStatelessActionAssetFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcMassStatelessActionAssetFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
