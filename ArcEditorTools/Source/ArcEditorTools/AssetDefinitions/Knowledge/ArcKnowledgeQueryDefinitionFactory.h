// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcKnowledgeQueryDefinitionFactory.generated.h"

UCLASS()
class UArcKnowledgeQueryDefinitionFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcKnowledgeQueryDefinitionFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
