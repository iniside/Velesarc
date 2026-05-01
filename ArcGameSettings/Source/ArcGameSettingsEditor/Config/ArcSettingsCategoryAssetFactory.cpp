// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSettingsCategoryAssetFactory.h"
#include "Widgets/ArcSettingsCategoryAsset.h"

UArcSettingsCategoryAssetFactory::UArcSettingsCategoryAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcSettingsCategoryAsset::StaticClass();
}

UObject* UArcSettingsCategoryAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcSettingsCategoryAsset>(InParent, Class, Name, Flags);
}
