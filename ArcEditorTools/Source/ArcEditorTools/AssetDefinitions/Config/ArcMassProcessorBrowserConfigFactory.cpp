// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/Config/ArcMassProcessorBrowserConfigFactory.h"
#include "MassProcessorBrowser/ArcMassProcessorBrowserConfig.h"

UArcMassProcessorBrowserConfigFactory::UArcMassProcessorBrowserConfigFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcMassProcessorBrowserConfig::StaticClass();
}

UObject* UArcMassProcessorBrowserConfigFactory::FactoryCreateNew(UClass* Class, UObject* InParent,
	FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn,
	FName CallingContext)
{
	return NewObject<UArcMassProcessorBrowserConfig>(InParent, Class, Name, Flags);
}
