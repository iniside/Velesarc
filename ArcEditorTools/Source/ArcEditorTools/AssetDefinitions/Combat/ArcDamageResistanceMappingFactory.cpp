// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/Combat/ArcDamageResistanceMappingFactory.h"
#include "ArcDamageSystem/ArcDamageResistanceMappingAsset.h"

UArcDamageResistanceMappingFactory::UArcDamageResistanceMappingFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcDamageResistanceMappingAsset::StaticClass();
}

UObject* UArcDamageResistanceMappingFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcDamageResistanceMappingAsset>(InParent, Class, Name, Flags);
}
