// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/ArcEcotopeRockModelFactory.h"
#include "Rocks/ArcEcotopeRockModel.h"

UArcEcotopeRockModelFactory::UArcEcotopeRockModelFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcEcotopeRockModel::StaticClass();
}

UObject* UArcEcotopeRockModelFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcEcotopeRockModel>(InParent, Class, Name, Flags);
}
