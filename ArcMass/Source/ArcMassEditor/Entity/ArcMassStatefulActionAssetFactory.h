// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcMassStatefulActionAssetFactory.generated.h"

UCLASS()
class UArcMassStatefulActionAssetFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcMassStatefulActionAssetFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
