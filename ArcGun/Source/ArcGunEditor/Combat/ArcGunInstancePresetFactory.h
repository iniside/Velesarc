// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcGunInstancePresetFactory.generated.h"

UCLASS()
class UArcGunInstancePresetFactory : public UFactory
{
	GENERATED_BODY()
public:
	UArcGunInstancePresetFactory(const FObjectInitializer& ObjectInitializer);
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
