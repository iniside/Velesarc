// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcGunRecoilInstancePresetFactory.generated.h"

UCLASS()
class UArcGunRecoilInstancePresetFactory : public UFactory
{
	GENERATED_BODY()
public:
	UArcGunRecoilInstancePresetFactory(const FObjectInitializer& ObjectInitializer);
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
