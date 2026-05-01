// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/Economy/ArcSettlementNeedDataAssetFactory.h"
#include "Data/ArcSettlementNeedDataAsset.h"

UArcSettlementNeedDataAssetFactory::UArcSettlementNeedDataAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcSettlementNeedDataAsset::StaticClass();
}

UObject* UArcSettlementNeedDataAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcSettlementNeedDataAsset>(InParent, Class, Name, Flags);
}
