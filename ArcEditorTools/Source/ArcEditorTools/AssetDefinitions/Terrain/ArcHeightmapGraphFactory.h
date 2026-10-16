// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcHeightmapGraphFactory.generated.h"

UCLASS()
class UArcHeightmapGraphFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcHeightmapGraphFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
