// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcKnowledgeEntryDefinitionFactory.generated.h"

UCLASS()
class UArcKnowledgeEntryDefinitionFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcKnowledgeEntryDefinitionFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
