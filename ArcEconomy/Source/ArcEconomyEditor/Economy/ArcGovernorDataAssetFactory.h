// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcGovernorDataAssetFactory.generated.h"

UCLASS()
class UArcGovernorDataAssetFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcGovernorDataAssetFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
