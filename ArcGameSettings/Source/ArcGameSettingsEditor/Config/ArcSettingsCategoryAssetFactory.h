// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcSettingsCategoryAssetFactory.generated.h"

UCLASS()
class UArcSettingsCategoryAssetFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcSettingsCategoryAssetFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
