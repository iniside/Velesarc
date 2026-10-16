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

// UArcInputActionConfig_Factory retired with the ArcCore Input cluster (Bucket B Cluster 1).

// PawnData asset definition / factory retired with UArcPawnData (Phase B).
// Experience asset definitions/factories retired with the Experience cluster (Bucket B Cluster 2).

// UArcAbilitySet_Factory retired with GAS keystone (Bucket D 7).

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
		, UBlueprint::StaticClass());

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
			KISMET_COMPILER_MODULENAME);
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

// UArcItemsStoreComponent_Factory + FArcAssetTypeActions_ItemsStoreComponent retired
// with UArcItemsStoreComponent (plan step 8c).
