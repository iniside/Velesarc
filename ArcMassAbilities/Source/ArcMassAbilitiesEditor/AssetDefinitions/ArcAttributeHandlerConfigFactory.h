// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcAttributeHandlerConfigFactory.generated.h"

UCLASS()
class UArcAttributeHandlerConfigFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcAttributeHandlerConfigFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
