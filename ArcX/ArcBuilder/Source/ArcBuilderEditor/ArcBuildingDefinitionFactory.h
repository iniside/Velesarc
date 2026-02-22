// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcBuildingDefinitionFactory.generated.h"

/**
 * Factory for creating new blank UArcBuildingDefinition assets from the Content Browser "Add" menu.
 */
UCLASS()
class ARCBUILDEREDITOR_API UArcBuildingDefinitionFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcBuildingDefinitionFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
