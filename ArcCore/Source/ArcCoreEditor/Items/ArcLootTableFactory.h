// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcLootTableFactory.generated.h"

UCLASS()
class ARCCOREEDITOR_API UArcLootTableFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcLootTableFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
