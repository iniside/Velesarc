// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcMassProcessorBrowserConfigFactory.generated.h"

UCLASS()
class UArcMassProcessorBrowserConfigFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcMassProcessorBrowserConfigFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
