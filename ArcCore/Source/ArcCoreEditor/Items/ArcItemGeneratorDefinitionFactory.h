// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcItemGeneratorDefinitionFactory.generated.h"

UCLASS()
class ARCCOREEDITOR_API UArcItemGeneratorDefinitionFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcItemGeneratorDefinitionFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
