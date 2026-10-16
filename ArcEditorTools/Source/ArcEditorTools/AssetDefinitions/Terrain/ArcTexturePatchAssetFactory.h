// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcTexturePatchAssetFactory.generated.h"

UCLASS()
class UArcTexturePatchAssetFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcTexturePatchAssetFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
