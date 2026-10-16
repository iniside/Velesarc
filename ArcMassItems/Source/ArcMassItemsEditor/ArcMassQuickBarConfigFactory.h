// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcMassQuickBarConfigFactory.generated.h"

UCLASS()
class ARCMASSITEMSEDITOR_API UArcMassQuickBarConfigFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcMassQuickBarConfigFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
