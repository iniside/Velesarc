// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcUserFacingExperienceDefinitionFactory.generated.h"

UCLASS()
class ARCCOREEDITOR_API UArcUserFacingExperienceDefinitionFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcUserFacingExperienceDefinitionFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
