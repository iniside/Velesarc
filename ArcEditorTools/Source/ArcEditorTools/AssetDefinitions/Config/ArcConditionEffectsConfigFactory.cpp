// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/Config/ArcConditionEffectsConfigFactory.h"
#include "ArcConditionEffects/ArcConditionEffectsConfig.h"

UArcConditionEffectsConfigFactory::UArcConditionEffectsConfigFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcConditionEffectsConfig::StaticClass();
}

UObject* UArcConditionEffectsConfigFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcConditionEffectsConfig>(InParent, Class, Name, Flags);
}
