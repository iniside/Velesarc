// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassItemAttachmentConfigFactory.h"
#include "Attachments/ArcMassItemAttachmentConfig.h"

UArcMassItemAttachmentConfigFactory::UArcMassItemAttachmentConfigFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcMassItemAttachmentConfig::StaticClass();
}

UObject* UArcMassItemAttachmentConfigFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcMassItemAttachmentConfig>(InParent, Class, Name, Flags);
}
