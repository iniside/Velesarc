// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcMassExperienceDefinitionFactory.generated.h"

UCLASS()
class ARCMASSGAMEPLAYFRAMEWORKEDITOR_API UArcMassExperienceDefinitionFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcMassExperienceDefinitionFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
