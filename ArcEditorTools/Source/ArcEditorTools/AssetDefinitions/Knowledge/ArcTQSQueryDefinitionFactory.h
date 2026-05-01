// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcTQSQueryDefinitionFactory.generated.h"

UCLASS()
class UArcTQSQueryDefinitionFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcTQSQueryDefinitionFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
