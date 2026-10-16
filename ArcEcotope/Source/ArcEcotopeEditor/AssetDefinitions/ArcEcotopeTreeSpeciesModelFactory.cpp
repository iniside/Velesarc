// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/ArcEcotopeTreeSpeciesModelFactory.h"
#include "Species/ArcEcotopeSpeciesModel.h"

UArcEcotopeTreeSpeciesModelFactory::UArcEcotopeTreeSpeciesModelFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcEcotopeTreeSpeciesModel::StaticClass();
}

UObject* UArcEcotopeTreeSpeciesModelFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcEcotopeTreeSpeciesModel>(InParent, Class, Name, Flags);
}
