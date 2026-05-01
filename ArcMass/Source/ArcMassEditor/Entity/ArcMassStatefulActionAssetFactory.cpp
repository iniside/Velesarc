// Copyright Lukasz Baran. All Rights Reserved.

#include "Entity/ArcMassStatefulActionAssetFactory.h"
#include "ArcMass/Actions/ArcMassActionAsset.h"

UArcMassStatefulActionAssetFactory::UArcMassStatefulActionAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcMassStatefulActionAsset::StaticClass();
}

UObject* UArcMassStatefulActionAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcMassStatefulActionAsset>(InParent, Class, Name, Flags);
}
