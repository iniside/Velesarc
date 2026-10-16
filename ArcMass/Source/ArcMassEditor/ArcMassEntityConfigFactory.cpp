/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
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

#include "ArcMassEntityConfigFactory.h"

#include "ArcMass/Config/ArcMassEntityConfigAsset.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "ArcMassEntityConfigFactory"

UArcMassEntityConfigFactory::UArcMassEntityConfigFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UArcMassEntityConfigAsset::StaticClass();
}

bool UArcMassEntityConfigFactory::ConfigureProperties()
{
	SourceTemplate = nullptr;

	IContentBrowserSingleton& CBS = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();

	FAssetPickerConfig PickerConfig;
	PickerConfig.Filter.ClassPaths.Add(UArcMassEntityConfigTemplate::StaticClass()->GetClassPathName());
	PickerConfig.Filter.bRecursiveClasses = false;
	PickerConfig.SelectionMode = ESelectionMode::Single;
	PickerConfig.InitialAssetViewType = EAssetViewType::Column;
	PickerConfig.bAllowNullSelection = true;
	PickerConfig.bShowBottomToolbar = false;
	PickerConfig.bAddFilterUI = false;

	bool bPressedOk = false;
	FAssetData OutAssetData;

	PickerConfig.OnAssetSelected.BindLambda([&OutAssetData](const FAssetData& AssetData)
	{
		OutAssetData = AssetData;
	});
	PickerConfig.OnAssetDoubleClicked.BindLambda([&OutAssetData, &bPressedOk](const FAssetData& AssetData)
	{
		OutAssetData = AssetData;
		bPressedOk = true;
		FSlateApplication::Get().DismissAllMenus();
	});

	TSharedRef<SWindow> PickerWindow = SNew(SWindow)
		.Title(LOCTEXT("PickTemplateTitle", "Pick Entity Config Template (optional, cancel for blank)"))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(700.f, 500.f))
		.SupportsMaximize(true)
		.SupportsMinimize(false);

	TSharedPtr<SWidget> PickerWidget = CBS.CreateAssetPicker(PickerConfig);

	PickerWindow->SetContent(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(4)
		[
			PickerWidget.ToSharedRef()
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)
			[
				SNew(SButton)
				.Text(LOCTEXT("OK", "OK"))
				.OnClicked_Lambda([&bPressedOk, &PickerWindow]() -> FReply
				{
					bPressedOk = true;
					PickerWindow->RequestDestroyWindow();
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)
			[
				SNew(SButton)
				.Text(LOCTEXT("Cancel", "Cancel"))
				.OnClicked_Lambda([&bPressedOk, &PickerWindow]() -> FReply
				{
					bPressedOk = false;
					PickerWindow->RequestDestroyWindow();
					return FReply::Handled();
				})
			]
		]
	);

	GEditor->EditorAddModalWindow(PickerWindow);

	if (!bPressedOk)
	{
		return false;
	}

	if (OutAssetData.IsValid())
	{
		SourceTemplate = Cast<UArcMassEntityConfigTemplate>(OutAssetData.GetAsset());
	}
	return true;
}

UObject* UArcMassEntityConfigFactory::FactoryCreateNew(UClass* Class
	, UObject* InParent
	, FName Name
	, EObjectFlags Flags
	, UObject* Context
	, FFeedbackContext* Warn
	, FName CallingContext)
{
	UArcMassEntityConfigAsset* NewVariant = NewObject<UArcMassEntityConfigAsset>(InParent, Class, Name, Flags);
	if (SourceTemplate && NewVariant)
	{
		SourceTemplate->SetNewOrReplaceTemplate(NewVariant);
	}
	return NewVariant;
}

#undef LOCTEXT_NAMESPACE
