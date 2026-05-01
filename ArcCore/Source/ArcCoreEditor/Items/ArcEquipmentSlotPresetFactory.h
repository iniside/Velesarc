// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcEquipmentSlotPresetFactory.generated.h"

UCLASS()
class ARCCOREEDITOR_API UArcEquipmentSlotPresetFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcEquipmentSlotPresetFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
