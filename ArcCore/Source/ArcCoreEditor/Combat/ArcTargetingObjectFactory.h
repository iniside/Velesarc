// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcTargetingObjectFactory.generated.h"

UCLASS()
class ARCCOREEDITOR_API UArcTargetingObjectFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcTargetingObjectFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
