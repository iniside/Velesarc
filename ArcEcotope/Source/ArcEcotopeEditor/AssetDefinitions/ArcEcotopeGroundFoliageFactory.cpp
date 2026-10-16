// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/ArcEcotopeGroundFoliageFactory.h"
#include "Foliage/ArcEcotopeGroundFoliage.h"

UArcEcotopeGroundFoliageFactory::UArcEcotopeGroundFoliageFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcEcotopeGroundFoliage::StaticClass();
}

UObject* UArcEcotopeGroundFoliageFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcEcotopeGroundFoliage>(InParent, Class, Name, Flags);
}
