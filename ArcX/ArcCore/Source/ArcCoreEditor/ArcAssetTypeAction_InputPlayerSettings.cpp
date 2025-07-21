/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "ArcAssetTypeAction_InputPlayerSettings.h"
#include "AssetRegistry/AssetDataTagMap.h"
#include "ClassViewerModule.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/SClassPickerDialog.h"
#include "KismetCompilerModule.h"
#include "ToolMenuSection.h"

#include "ClassViewerFilter.h"
#include "SDetailsDiff.h"
#include "Items/ArcItemsStoreComponent.h"

UArcInputActionConfig_Factory::UArcInputActionConfig_Factory()
{
	SupportedClass = UArcInputActionConfig::StaticClass();
	bEditAfterNew = true;
	bCreateNew = true;
}

UObject* UArcInputActionConfig_Factory::FactoryCreateNew(UClass* Class
														 , UObject* InParent
														 , FName Name
														 , EObjectFlags Flags
														 , UObject* Context
														 , FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UArcInputActionConfig::StaticClass()));
	return NewObject<UArcInputActionConfig>(InParent
		, Class
		, Name
		, Flags | RF_Transactional
		, Context);
}

EAssetCommandResult UArcAssetDefinition_PawnData::PerformAssetDiff(const FAssetDiffArgs& DiffArgs) const
{
	const bool bIsSingleAsset = (DiffArgs.OldAsset->GetFName() == DiffArgs.NewAsset->GetFName());
	static const FText BasicWindowTitle = FText::FromString("Arc PawnData Diff");

	const FText WindowTitle = FText::FromString("Arc PawnData Diff");
	// !bIsSingleAsset ? BasicWindowTitle : FText::Format(LOCTEXT("DataAsset Diff", "{0} - DataAsset Diff"), FText::FromString(DiffArgs.NewAsset->GetName()));

	SDetailsDiff::CreateDiffWindow(WindowTitle
		, DiffArgs.OldAsset
		, DiffArgs.NewAsset
		, DiffArgs.OldRevision
		, DiffArgs.NewRevision);

	return EAssetCommandResult::Handled;
}

UArcPawnData_Factory::UArcPawnData_Factory()
{
	SupportedClass = UArcPawnData::StaticClass();
	bEditAfterNew = true;
	bCreateNew = true;
}

UObject* UArcPawnData_Factory::FactoryCreateNew(UClass* Class
												, UObject* InParent
												, FName Name
												, EObjectFlags Flags
												, UObject* Context
												, FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UArcPawnData::StaticClass()));
	return NewObject<UArcPawnData>(InParent
		, Class
		, Name
		, Flags | RF_Transactional
		, Context);
}

UArcExperienceActionSet_Factory::UArcExperienceActionSet_Factory()
{
	SupportedClass = UArcExperienceActionSet::StaticClass();
	bEditAfterNew = true;
	bCreateNew = true;
}

UObject* UArcExperienceActionSet_Factory::FactoryCreateNew(UClass* Class
	, UObject* InParent
	, FName Name
	, EObjectFlags Flags
	, UObject* Context
	, FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UArcExperienceActionSet::StaticClass()));
	return NewObject<UArcExperienceActionSet>(InParent
		, Class
		, Name
		, Flags | RF_Transactional
		, Context);
}

UArcExperienceDefinition_Factory::UArcExperienceDefinition_Factory()
{
	SupportedClass = UArcExperienceDefinition::StaticClass();
	bEditAfterNew = true;
	bCreateNew = true;
}

UObject* UArcExperienceDefinition_Factory::FactoryCreateNew(UClass* Class
															, UObject* InParent
															, FName Name
															, EObjectFlags Flags
															, UObject* Context
															, FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UArcExperienceDefinition::StaticClass()));
	return NewObject<UArcExperienceDefinition>(InParent
		, Class
		, Name
		, Flags | RF_Transactional
		, Context);
}

UArcExperienceData_Factory::UArcExperienceData_Factory()
{
	SupportedClass = UArcExperienceData::StaticClass();
	bEditAfterNew = true;
	bCreateNew = true;
}

UObject* UArcExperienceData_Factory::FactoryCreateNew(UClass* Class
													  , UObject* InParent
													  , FName Name
													  , EObjectFlags Flags
													  , UObject* Context
													  , FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UArcExperienceData::StaticClass()));
	return NewObject<UArcExperienceData>(InParent
		, Class
		, Name
		, Flags | RF_Transactional
		, Context);
}

UArcAbilitySet_Factory::UArcAbilitySet_Factory()
{
	SupportedClass = UArcAbilitySet::StaticClass();
	bEditAfterNew = true;
	bCreateNew = true;
}

UObject* UArcAbilitySet_Factory::FactoryCreateNew(UClass* Class
												  , UObject* InParent
												  , FName Name
												  , EObjectFlags Flags
												  , UObject* Context
												  , FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UArcAbilitySet::StaticClass()));
	return NewObject<UArcAbilitySet>(InParent
		, Class
		, Name
		, Flags | RF_Transactional
		, Context);
}

class FArcBlueprintParentFilter : public IClassViewerFilter
{
public:
	/** Classes to not allow any children of into the Class Viewer/Picker. */
	TSet<const UClass*> AllowedClasses;

	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions
								, const UClass* InClass
								, TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override
	{
		return InFilterFuncs->IfInChildOfClassesSet(AllowedClasses
				   , InClass) == EFilterReturn::Passed && !InClass->HasAnyClassFlags(CLASS_Deprecated);
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions
										, const TSharedRef<const IUnloadedBlueprintData> InUnloadedClassData
										, TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override
	{
		return InFilterFuncs->IfInChildOfClassesSet(AllowedClasses
				   , InUnloadedClassData) == EFilterReturn::Passed && !InUnloadedClassData->HasAnyClassFlags(
				   CLASS_Deprecated);
	}
};

UArcComponentFactory::UArcComponentFactory()
{
	BlueprintClassType = UBlueprint::StaticClass();
}

bool UArcComponentFactory::ConfigureProperties()
{
	if (bSkipClassPicker)
	{
		check(ParentClass);
		return true;
	}

	// Null the parent class to ensure one is selected
	ParentClass = nullptr;

	// Fill in options
	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.DisplayMode = EClassViewerDisplayMode::ListView;
	Options.bShowDefaultClasses = false;

	Options.bShowObjectRootClass = true;

	// Only want blueprint actor base classes.
	Options.bIsBlueprintBaseOnly = true;

	// This will allow unloaded blueprints to be shown.
	Options.bShowUnloadedBlueprints = true;

	// Enable Class Dynamic Loading
	Options.bEnableClassDynamicLoading = true;

	Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::Dynamic;

	// Prevent creating blueprints of classes that require special setup (they'll be
	// allowed in the corresponding factories / via other means)
	TSharedPtr<FArcBlueprintParentFilter> Filter = MakeShareable(new FArcBlueprintParentFilter);
	Filter->AllowedClasses.Add(BaseClass);
	Options.ClassFilters.Add(Filter.ToSharedRef());

	// Allow overriding properties
	// OnConfigurePropertiesDelegate.ExecuteIfBound(&Options);

	Options.ExtraPickerCommonClasses.Empty();

	const FText TitleText = FText::FromString("Pick Parent Class");
	UClass* ChosenClass = nullptr;
	const bool bPressedOk = SClassPickerDialog::PickClass(TitleText
		, Options
		, ChosenClass
		, UArcQuickBarComponentBlueprint::StaticClass());

	if (bPressedOk)
	{
		ParentClass = ChosenClass;

		FEditorDelegates::OnFinishPickingBlueprintClass.Broadcast(ParentClass);
	}

	return bPressedOk;
}

UObject* UArcComponentFactory::FactoryCreateNew(UClass* Class
												, UObject* InParent
												, FName Name
												, EObjectFlags Flags
												, UObject* Context
												, FFeedbackContext* Warn
												, FName CallingContext)
{
	check(Class->IsChildOf(UBlueprint::StaticClass()));

	if ((ParentClass == nullptr) || !FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ClassName")
			, (ParentClass != nullptr) ? FText::FromString(ParentClass->GetName()) : FText::FromString("(null)"));
		FMessageDialog::Open(EAppMsgType::Ok
			, FText::Format(FText::FromString("Cannot create a blueprint based on the class '{0}'.")
				, Args));
		return nullptr;
	}
	else
	{
		UClass* BlueprintClass = nullptr;
		UClass* BlueprintGeneratedClass = nullptr;

		IKismetCompilerInterface& KismetCompilerModule = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>(
			"KismetCompiler");
		KismetCompilerModule.GetBlueprintTypesForClass(ParentClass
			, BlueprintClass
			, BlueprintGeneratedClass);

		return FKismetEditorUtilities::CreateBlueprint(ParentClass
			, InParent
			, Name
			, BPTYPE_Normal
			, BlueprintClassType
			, BlueprintGeneratedClass
			, CallingContext);
	}
}

UArcQuickBarComponent_Factory::UArcQuickBarComponent_Factory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	BlueprintClassType = UArcQuickBarComponentBlueprint::StaticClass();
	SupportedClass = UArcQuickBarComponentBlueprint::StaticClass();
	ParentClass = UArcQuickBarComponent::StaticClass();
	BaseClass = UArcQuickBarComponent::StaticClass();
}

UFactory* FArcAssetTypeActions_QuickBarComponent::GetFactoryForBlueprintType(UBlueprint* InBlueprint) const
{
	UArcQuickBarComponent_Factory* GameplayAbilitiesBlueprintFactory = NewObject<UArcQuickBarComponent_Factory>();
	GameplayAbilitiesBlueprintFactory->ParentClass = TSubclassOf<UArcQuickBarComponent>(*InBlueprint->GeneratedClass);
	return GameplayAbilitiesBlueprintFactory;
}

UArcItemsStoreComponent_Factory::UArcItemsStoreComponent_Factory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	BlueprintClassType = UArcItemsStoreComponentBlueprint::StaticClass();
	SupportedClass = UArcItemsStoreComponentBlueprint::StaticClass();
	ParentClass = UArcItemsStoreComponent::StaticClass();
	BaseClass = UArcItemsStoreComponent::StaticClass();
}

UFactory* FArcAssetTypeActions_ItemsStoreComponent::GetFactoryForBlueprintType(UBlueprint* InBlueprint) const
{
	UArcItemsStoreComponent_Factory* GameplayAbilitiesBlueprintFactory = NewObject<UArcItemsStoreComponent_Factory>();
	GameplayAbilitiesBlueprintFactory->ParentClass = TSubclassOf<UArcItemsStoreComponent>(*InBlueprint->GeneratedClass);
	return GameplayAbilitiesBlueprintFactory;
}
