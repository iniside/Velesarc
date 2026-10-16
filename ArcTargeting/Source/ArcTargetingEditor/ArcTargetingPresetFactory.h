// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcTargetingPresetFactory.generated.h"

UCLASS()
class ARCTARGETINGEDITOR_API UArcTargetingPresetFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcTargetingPresetFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
