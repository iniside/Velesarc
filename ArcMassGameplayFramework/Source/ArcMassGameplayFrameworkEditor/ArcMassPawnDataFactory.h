// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcMassPawnDataFactory.generated.h"

UCLASS()
class ARCMASSGAMEPLAYFRAMEWORKEDITOR_API UArcMassPawnDataFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcMassPawnDataFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
