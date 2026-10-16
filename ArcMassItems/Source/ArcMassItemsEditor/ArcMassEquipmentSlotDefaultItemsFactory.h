// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcMassEquipmentSlotDefaultItemsFactory.generated.h"

UCLASS()
class ARCMASSITEMSEDITOR_API UArcMassEquipmentSlotDefaultItemsFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcMassEquipmentSlotDefaultItemsFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
