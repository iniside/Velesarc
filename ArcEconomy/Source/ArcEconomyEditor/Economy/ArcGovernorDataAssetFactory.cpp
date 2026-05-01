// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGovernorDataAssetFactory.h"
#include "Data/ArcGovernorDataAsset.h"

UArcGovernorDataAssetFactory::UArcGovernorDataAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcGovernorDataAsset::StaticClass();
}

UObject* UArcGovernorDataAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcGovernorDataAsset>(InParent, Class, Name, Flags);
}
