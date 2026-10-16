// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/ArcEcotopeLowGroundCoverSpeciesModelFactory.h"
#include "Species/ArcEcotopeSpeciesModel.h"

UArcEcotopeLowGroundCoverSpeciesModelFactory::UArcEcotopeLowGroundCoverSpeciesModelFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcEcotopeLowGroundCoverSpeciesModel::StaticClass();
}

UObject* UArcEcotopeLowGroundCoverSpeciesModelFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcEcotopeLowGroundCoverSpeciesModel>(InParent, Class, Name, Flags);
}
