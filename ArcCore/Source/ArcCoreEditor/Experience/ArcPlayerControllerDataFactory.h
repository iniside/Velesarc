// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcPlayerControllerDataFactory.generated.h"

UCLASS()
class ARCCOREEDITOR_API UArcPlayerControllerDataFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcPlayerControllerDataFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
