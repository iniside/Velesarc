// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ArcMassItemAttachmentConfigFactory.generated.h"

UCLASS()
class ARCMASSITEMSEDITOR_API UArcMassItemAttachmentConfigFactory : public UFactory
{
	GENERATED_BODY()

public:
	UArcMassItemAttachmentConfigFactory(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
		EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
		FName CallingContext) override;
};
