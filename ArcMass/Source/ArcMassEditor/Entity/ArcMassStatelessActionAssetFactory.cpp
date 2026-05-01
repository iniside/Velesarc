// Copyright Lukasz Baran. All Rights Reserved.

#include "Entity/ArcMassStatelessActionAssetFactory.h"
#include "ArcMass/Actions/ArcMassActionAsset.h"

UArcMassStatelessActionAssetFactory::UArcMassStatelessActionAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcMassStatelessActionAsset::StaticClass();
}

UObject* UArcMassStatelessActionAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcMassStatelessActionAsset>(InParent, Class, Name, Flags);
}
