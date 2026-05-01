// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcGunFireModePresetFactory.generated.h"

UCLASS()
class UArcGunFireModePresetFactory : public UFactory
{
	GENERATED_BODY()
public:
	UArcGunFireModePresetFactory(const FObjectInitializer& ObjectInitializer);
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
