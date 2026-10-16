// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcMassEquipmentSlotPresetFactory.generated.h"

UCLASS()
class ARCMASSITEMSEDITOR_API UArcMassEquipmentSlotPresetFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcMassEquipmentSlotPresetFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
