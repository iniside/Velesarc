// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/ArcEcotopeModelFactory.h"
#include "Ecotope/ArcEcotopeModel.h"

UArcEcotopeModelFactory::UArcEcotopeModelFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcEcotopeModel::StaticClass();
}

UObject* UArcEcotopeModelFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcEcotopeModel>(InParent, Class, Name, Flags);
}
