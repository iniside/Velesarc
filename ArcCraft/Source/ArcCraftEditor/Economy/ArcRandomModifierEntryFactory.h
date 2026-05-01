// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcRandomModifierEntryFactory.generated.h"

UCLASS()
class ARCCRAFTEDITOR_API UArcRandomModifierEntryFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcRandomModifierEntryFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
