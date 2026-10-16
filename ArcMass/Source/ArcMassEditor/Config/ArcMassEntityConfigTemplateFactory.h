// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcMassEntityConfigTemplateFactory.generated.h"

UCLASS()
class UArcMassEntityConfigTemplateFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcMassEntityConfigTemplateFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class
		, UObject* InParent
		, FName Name
		, EObjectFlags Flags
		, UObject* Context
		, FFeedbackContext* Warn
		, FName CallingContext) override;
};
