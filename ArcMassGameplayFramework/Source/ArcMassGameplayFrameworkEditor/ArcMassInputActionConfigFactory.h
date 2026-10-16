// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcMassInputActionConfigFactory.generated.h"

UCLASS()
class ARCMASSGAMEPLAYFRAMEWORKEDITOR_API UArcMassInputActionConfigFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcMassInputActionConfigFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
