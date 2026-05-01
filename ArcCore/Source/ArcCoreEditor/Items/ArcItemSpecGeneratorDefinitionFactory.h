// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcItemSpecGeneratorDefinitionFactory.generated.h"

UCLASS()
class ARCCOREEDITOR_API UArcItemSpecGeneratorDefinitionFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcItemSpecGeneratorDefinitionFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
