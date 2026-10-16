// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassAbilitiesEditorModule.h"
#include "PropertyEditorModule.h"
#include "Customizations/ArcAttributeRefCustomization.h"
#include "Customizations/ArcScopedModifierCustomization.h"
#include "Customizations/ArcEffectModifierCustomization.h"
#include "Customizations/ArcEffectExecutionEntryCustomization.h"
#include "Customizations/ArcExtraCaptureDefCustomization.h"
#include "Attributes/ArcAttribute.h"
#include "Effects/ArcEffectExecution.h"
#include "Effects/ArcEffectModifier.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcExtraCapture.h"

#include "Debugger/ArcMassAbilityTraceModule.h"
#include "Debugger/ArcMassAbilityRewindDebuggerTrack.h"
#include "Debugger/ArcMassAbilityRewindDebuggerExtensions.h"
#include "Features/IModularFeatures.h"
#include "IRewindDebuggerExtension.h"
#include "IRewindDebuggerTrackCreator.h"
#include "RewindDebuggerRuntimeInterface/IRewindDebuggerRuntimeExtension.h"
#include "TraceServices/ModuleService.h"
#include "StateTreeEditorModule.h"
#include "Schema/ArcAbilityStateTreeEditorSchema.h"
#include "Abilities/Schema/ArcAbilityStateTreeSchema.h"
#include "Extensions/ArcAbilityPresetToolbarExtension.h"
#include "ToolMenus.h"

#include "Styling/SlateStyleRegistry.h"
#include "Brushes/SlateImageBrush.h"
#include "Interfaces/IPluginManager.h"

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
	PropertyModule.RegisterCustomPropertyTypeLayout(
		FArcEffectModifier::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FArcEffectModifierCustomization::MakeInstance)
	);
	PropertyModule.RegisterCustomPropertyTypeLayout(
		FArcEffectExecutionEntry::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FArcEffectExecutionEntryCustomization::MakeInstance)
	);
	PropertyModule.RegisterCustomPropertyTypeLayout(
		FArcExtraCaptureDef::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FArcExtraCaptureDefCustomization::MakeInstance)
	);

	// --- ArcMassAbility Rewind Debugger ---

	ArcMABTraceModule = MakeUnique<FArcMassAbilityTraceModule>();
	IModularFeatures::Get().RegisterModularFeature(TraceServices::ModuleFeatureName, ArcMABTraceModule.Get());

	ArcMABTrackCreator = MakePimpl<FArcMassAbilityRewindDebuggerTrackCreator>();
	IModularFeatures::Get().RegisterModularFeature(
		RewindDebugger::IRewindDebuggerTrackCreator::ModularFeatureName,
		ArcMABTrackCreator.Get());

	ArcMABPlaybackExtension = MakeUnique<FArcMassAbilityRewindDebuggerPlaybackExtension>();
	IModularFeatures::Get().RegisterModularFeature(
		IRewindDebuggerExtension::ModularFeatureName,
		ArcMABPlaybackExtension.Get());

	ArcMABRecordingExtension = MakeUnique<FArcMassAbilityRewindDebuggerRecordingExtension>();
	IModularFeatures::Get().RegisterModularFeature(
		IRewindDebuggerRuntimeExtension::ModularFeatureName,
		ArcMABRecordingExtension.Get());

	FStateTreeEditorModule::GetModule().RegisterEditorSchemaClass(
		UArcAbilityStateTreeSchema::StaticClass(),
		UArcAbilityStateTreeEditorSchema::StaticClass());

	// RegisterStartupCallback holds the extension back until ToolMenus is ready, and drops it entirely in commandlet/game runs (ToolMenus.h:140-145).
	PresetToolbarStartupHandle = UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateStatic(&ArcAbilityPresetToolbarExtension::RegisterMenus));

	if (!StyleSet.IsValid())
	{
		const FVector2D Icon16x16(16.0f, 16.0f);
		const FVector2D Icon64x64(64.0f, 64.0f);
		const FString IconRoot = IPluginManager::Get().FindPlugin(TEXT("ArcMassAbilities"))->GetBaseDir()
			+ TEXT("/Content/Editor/AssetIcons");

		StyleSet = MakeShared<FSlateStyleSet>(TEXT("ArcMassAbilitiesEditor"));

		StyleSet->Set(TEXT("ClassThumbnail.ArcAbilityDefinition"),
			new FSlateVectorImageBrush(IconRoot + TEXT("/ArcAbilityDefinition.svg"), Icon64x64));
		StyleSet->Set(TEXT("ClassIcon.ArcAbilityDefinition"),
			new FSlateVectorImageBrush(IconRoot + TEXT("/ArcAbilityDefinition.svg"), Icon16x16));

		StyleSet->Set(TEXT("ClassThumbnail.ArcEffectDefinition"),
			new FSlateVectorImageBrush(IconRoot + TEXT("/ArcEffectDefinition.svg"), Icon64x64));
		StyleSet->Set(TEXT("ClassIcon.ArcEffectDefinition"),
			new FSlateVectorImageBrush(IconRoot + TEXT("/ArcEffectDefinition.svg"), Icon16x16));

		StyleSet->Set(TEXT("ClassThumbnail.ArcAttributeHandlerConfig"),
			new FSlateVectorImageBrush(IconRoot + TEXT("/ArcAttributeHandlerConfig.svg"), Icon64x64));
		StyleSet->Set(TEXT("ClassIcon.ArcAttributeHandlerConfig"),
			new FSlateVectorImageBrush(IconRoot + TEXT("/ArcAttributeHandlerConfig.svg"), Icon16x16));

		StyleSet->Set(TEXT("ClassThumbnail.ArcMassAbilitySet"),
			new FSlateVectorImageBrush(IconRoot + TEXT("/ArcMassAbilitySet.svg"), Icon64x64));
		StyleSet->Set(TEXT("ClassIcon.ArcMassAbilitySet"),
			new FSlateVectorImageBrush(IconRoot + TEXT("/ArcMassAbilitySet.svg"), Icon16x16));

		FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
	}
}

void FArcMassAbilitiesEditorModule::ShutdownModule()
{
	// On full engine exit the UObject array is torn down before modules unload, so any
	// StaticClass()/StaticStruct() call here asserts in IndexToObject. Nothing to clean up
	// when the process is going away — only Hot Reload / Live Coding needs the unregisters.
	if (IsEngineExitRequested())
	{
		return;
	}

	UToolMenus::UnRegisterStartupCallback(PresetToolbarStartupHandle);
	UToolMenus::UnregisterOwner(UE_MODULE_NAME);

	if (FModuleManager::Get().IsModuleLoaded("StateTreeEditorModule"))
	{
		FStateTreeEditorModule::GetModule().UnregisterEditorSchemaClass(
			UArcAbilityStateTreeSchema::StaticClass());
	}

	// --- ArcMassAbility Rewind Debugger ---

	if (ArcMABRecordingExtension)
	{
		IModularFeatures::Get().UnregisterModularFeature(
			IRewindDebuggerRuntimeExtension::ModularFeatureName,
			ArcMABRecordingExtension.Get());
	}

	if (ArcMABPlaybackExtension)
	{
		IModularFeatures::Get().UnregisterModularFeature(
			IRewindDebuggerExtension::ModularFeatureName,
			ArcMABPlaybackExtension.Get());
	}

	if (ArcMABTrackCreator)
	{
		IModularFeatures::Get().UnregisterModularFeature(
			RewindDebugger::IRewindDebuggerTrackCreator::ModularFeatureName,
			ArcMABTrackCreator.Get());
	}

	if (ArcMABTraceModule)
	{
		IModularFeatures::Get().UnregisterModularFeature(
			TraceServices::ModuleFeatureName,
			ArcMABTraceModule.Get());
	}

	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout(FArcAttributeRef::StaticStruct()->GetFName());
		PropertyModule.UnregisterCustomPropertyTypeLayout(FArcScopedModifier::StaticStruct()->GetFName());
		PropertyModule.UnregisterCustomPropertyTypeLayout(FArcEffectModifier::StaticStruct()->GetFName());
		PropertyModule.UnregisterCustomPropertyTypeLayout(FArcEffectExecutionEntry::StaticStruct()->GetFName());
		PropertyModule.UnregisterCustomPropertyTypeLayout(FArcExtraCaptureDef::StaticStruct()->GetFName());
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArcMassAbilitiesEditorModule, ArcMassAbilitiesEditor)
