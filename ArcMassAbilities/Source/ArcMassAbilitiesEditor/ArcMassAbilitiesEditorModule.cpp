// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassAbilitiesEditorModule.h"
#include "PropertyEditorModule.h"
#include "Customizations/ArcAttributeRefCustomization.h"
#include "Customizations/ArcScopedModifierCustomization.h"
#include "Attributes/ArcAttribute.h"
#include "Effects/ArcEffectExecution.h"

#define LOCTEXT_NAMESPACE "FArcMassAbilitiesEditorModule"

void FArcMassAbilitiesEditorModule::StartupModule()
{
	if (IsRunningGame())
	{
		return;
	}

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout(
		FArcAttributeRef::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FArcAttributeRefCustomization::MakeInstance)
	);
	PropertyModule.RegisterCustomPropertyTypeLayout(
		FArcScopedModifier::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FArcScopedModifierCustomization::MakeInstance)
	);
}

void FArcMassAbilitiesEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout(FArcAttributeRef::StaticStruct()->GetFName());
		PropertyModule.UnregisterCustomPropertyTypeLayout(FArcScopedModifier::StaticStruct()->GetFName());
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArcMassAbilitiesEditorModule, ArcMassAbilitiesEditor)
