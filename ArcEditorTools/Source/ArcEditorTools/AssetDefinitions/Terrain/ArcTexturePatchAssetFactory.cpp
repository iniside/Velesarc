// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/Terrain/ArcTexturePatchAssetFactory.h"
#include "TerrainTool/ArcTexturePatchAsset.h"

UArcTexturePatchAssetFactory::UArcTexturePatchAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcTexturePatchAsset::StaticClass();
}

UObject* UArcTexturePatchAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcTexturePatchAsset>(InParent, Class, Name, Flags);
}
