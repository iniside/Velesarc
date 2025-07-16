/**
 * This file is part of ArcX.
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

#include "ArcNamedPrimaryAssetIdCustomization.h"

#include "AssetManagerEditorModule.h"
#include "EdGraph/EdGraphSchema.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Engine/AssetManager.h"
#include "PropertyCustomizationHelpers.h"
#include "Editor.h"
#include "AssetThumbnail.h"
#include "ScopedTransaction.h"
#include "Widgets/SOverlay.h"

#define LOCTEXT_NAMESPACE "PrimaryAssetIdCustomization"
namespace Arcx::Editor
{
	TSharedRef<SWidget> MakePrimaryAssetIdSelector(FOnGetPrimaryAssetDisplayText OnGetDisplayText
		, FOnGetPrimaryAssetDisplayText OnGetTooltipText
		, FOnSetPrimaryAssetId OnSetId
		, bool bAllowClear
		, TArray<FPrimaryAssetType> AllowedTypes, TArray<const UClass*> AllowedClasses, TArray<const UClass*> DisallowedClasses
		, TSharedPtr<IPropertyHandle> InPropertyHandle
		, const TArray<FAssetData>& OwnerAssets)
	{
		FOnGetContent OnCreateMenuContent = FOnGetContent::CreateLambda([OwnerAssets, InPropertyHandle, OnGetDisplayText, OnSetId, bAllowClear, AllowedTypes, AllowedClasses, DisallowedClasses]()
		{
			FOnShouldFilterAsset AssetFilter = FOnShouldFilterAsset::CreateStatic(&IAssetManagerEditorModule::OnShouldFilterPrimaryAsset, AllowedTypes);
			FOnSetObject OnSetObject = FOnSetObject::CreateLambda([OnSetId](const FAssetData& AssetData)
			{
				FSlateApplication::Get().DismissAllMenus();
				UAssetManager& Manager = UAssetManager::Get();

				FPrimaryAssetId AssetId;
				if (AssetData.IsValid())
				{
					AssetId = Manager.GetPrimaryAssetIdForData(AssetData);
					ensure(AssetId.IsValid());
				}

				OnSetId.Execute(AssetId);
			});

			TArray<UFactory*> NewAssetFactories;

			return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
				FAssetData(),
				bAllowClear,
				AllowedClasses,
				DisallowedClasses,
				NewAssetFactories,
				AssetFilter,
				OnSetObject,
				FSimpleDelegate(),
				InPropertyHandle,
				OwnerAssets);
		});

		TAttribute<FText> OnGetObjectText = TAttribute<FText>::Create(OnGetDisplayText);
		TAttribute<FText> OnGetObjectTooltipText = TAttribute<FText>::Create(OnGetTooltipText);
		
		return SNew(SComboButton)
			.OnGetMenuContent(OnCreateMenuContent)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(OnGetObjectText)
				.ToolTipText(OnGetObjectTooltipText)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			];
	}
}
void FArcNamedPrimaryAssetIdCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	check(UAssetManager::IsInitialized());

	StructPropertyHandle = InStructPropertyHandle;
	AssetIdPropertyHandle = InStructPropertyHandle->GetChildHandle("AssetId");
	NamePropertyHandle = InStructPropertyHandle->GetChildHandle("DisplayName");

	TArray<UObject*> Outers;
	StructPropertyHandle->GetOuterObjects(Outers);
	TArray<FAssetData> OwningAssets;
	for (UObject* Out : Outers)
	{
		OwningAssets.Add(FAssetData(Out));
	}
	const FString& TypeFilterString = StructPropertyHandle->GetMetaData("AllowedTypes");
	if( !TypeFilterString.IsEmpty() )
	{
		TArray<FString> CustomTypeFilterNames;
		TypeFilterString.ParseIntoArray(CustomTypeFilterNames, TEXT(","), true);

		for(auto It = CustomTypeFilterNames.CreateConstIterator(); It; ++It)
		{
			const FString& TypeName = *It;

			AllowedTypes.Add(*TypeName);
		}
	}

	bool bShowThumbnail = true;
	if (StructPropertyHandle->HasMetaData("DisplayThumbnail"))
	{
		const FString& ShowThumbnailString = StructPropertyHandle->GetMetaData("DisplayThumbnail");
		bShowThumbnail = ShowThumbnailString.Len() == 0 || ShowThumbnailString == TEXT("true");
	}

	TSharedRef<SHorizontalBox> ValueBox = SNew(SHorizontalBox);

	if (bShowThumbnail)
	{
		const int32 ThumbnailSize = 64;
		AssetThumbnail = MakeShareable(new FAssetThumbnail(FAssetData(), ThumbnailSize, ThumbnailSize, StructCustomizationUtils.GetThumbnailPool()));
		UpdateThumbnail();

		ValueBox->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 4, 0)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				.Padding(1)
				[
					SAssignNew(ThumbnailBorder, SBorder)
					.Padding(0)
					.BorderImage(FStyleDefaults::GetNoBrush())
					.OnMouseDoubleClick(this, &FArcNamedPrimaryAssetIdCustomization::OnAssetThumbnailDoubleClick)
					[
						SNew(SBox)
						.WidthOverride(ThumbnailSize)
						.HeightOverride(ThumbnailSize)
						[
							AssetThumbnail->MakeThumbnailWidget()
						]
					]
				]
				+ SOverlay::Slot()
				[
					SNew(SImage)
					.Image(this, &FArcNamedPrimaryAssetIdCustomization::GetThumbnailBorder)
					.Visibility(EVisibility::SelfHitTestInvisible)
				]
			];
	}

	// Can the field be cleared
	const bool bAllowClear = !(StructPropertyHandle->GetMetaDataProperty()->PropertyFlags & CPF_NoClear);

	AllowedClasses = PropertyCustomizationHelpers::GetClassesFromMetadataString(StructPropertyHandle->GetMetaData("AllowedClasses"));
	DisallowedClasses = PropertyCustomizationHelpers::GetClassesFromMetadataString(StructPropertyHandle->GetMetaData("DisallowedClasses"));
	
	ValueBox->AddSlot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			Arcx::Editor::MakePrimaryAssetIdSelector(
				FOnGetPrimaryAssetDisplayText::CreateSP(this, &FArcNamedPrimaryAssetIdCustomization::GetDisplayText),
				FOnGetPrimaryAssetDisplayText::CreateSP(this, &FArcNamedPrimaryAssetIdCustomization::GetDisplayTooltipText),
				FOnSetPrimaryAssetId::CreateSP(this, &FArcNamedPrimaryAssetIdCustomization::OnIdSelected),
				bAllowClear, AllowedTypes, AllowedClasses, DisallowedClasses, AssetIdPropertyHandle, OwningAssets)
		];

	ValueBox->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeUseSelectedButton(FSimpleDelegate::CreateSP(this, &FArcNamedPrimaryAssetIdCustomization::OnUseSelected))
		];

	ValueBox->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeBrowseButton(FSimpleDelegate::CreateSP(this, &FArcNamedPrimaryAssetIdCustomization::OnBrowseTo))
		];

	ValueBox->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeClearButton(FSimpleDelegate::CreateSP(this, &FArcNamedPrimaryAssetIdCustomization::OnClear))
		];

	HeaderRow
	.NameContent()
	[
		InStructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(250.0f)
	.MaxDesiredWidth(0.0f)
	[
		ValueBox
	];
}

const FSlateBrush* FArcNamedPrimaryAssetIdCustomization::GetThumbnailBorder() const
{
	static const FName HoveredBorderName("PropertyEditor.AssetThumbnailBorderHovered");
	static const FName RegularBorderName("PropertyEditor.AssetThumbnailBorder");

	return ThumbnailBorder->IsHovered() ? FAppStyle::Get().GetBrush(HoveredBorderName) : FAppStyle::Get().GetBrush(RegularBorderName);
}

void FArcNamedPrimaryAssetIdCustomization::OnIdSelected(FPrimaryAssetId AssetId)
{
	if (AssetIdPropertyHandle.IsValid() && AssetIdPropertyHandle->IsValidHandle())
	{
		FAssetData AssetData;
		if (UAssetManager::Get().GetPrimaryAssetData(AssetId, AssetData))
		{
			NamePropertyHandle->SetValue(AssetData.AssetName.ToString());
		}
		else
		{
			NamePropertyHandle->SetValue(FString());
		}
		AssetIdPropertyHandle->SetValueFromFormattedString(AssetId.ToString());
	}

	UpdateThumbnail();
}

FText FArcNamedPrimaryAssetIdCustomization::GetDisplayText() const
{
	FString StringReference;
	if (AssetIdPropertyHandle.IsValid())
	{
		FAssetData AssetData;
		FPrimaryAssetId PrimaryAssetId = GetCurrentPrimaryAssetId();
		if (UAssetManager::Get().GetPrimaryAssetData(PrimaryAssetId, AssetData))
		{
			const FString AssetType = PrimaryAssetId.PrimaryAssetType.ToString();
			StringReference = AssetType + ":" + AssetData.AssetName.ToString();// + " \n(" + PrimaryAssetId.PrimaryAssetName.ToString() + ")";
		}
		else
		{
			AssetIdPropertyHandle->GetValueAsFormattedString(StringReference);	
		}
	}
	else
	{
		StringReference = FPrimaryAssetId().ToString();
	}

	return FText::AsCultureInvariant(StringReference);
}

FText FArcNamedPrimaryAssetIdCustomization::GetDisplayTooltipText() const
{
	FString StringReference;
	if (AssetIdPropertyHandle.IsValid())
	{
		AssetIdPropertyHandle->GetValueAsFormattedString(StringReference);
	}
	else
	{
		StringReference = FPrimaryAssetId().ToString();
	}

	return FText::AsCultureInvariant(StringReference);
}

FPrimaryAssetId FArcNamedPrimaryAssetIdCustomization::GetCurrentPrimaryAssetId() const
{
	FString StringReference;
	if (AssetIdPropertyHandle.IsValid())
	{
		AssetIdPropertyHandle->GetValueAsFormattedString(StringReference);
	}
	else
	{
		StringReference = FPrimaryAssetId().ToString();
	}

	return FPrimaryAssetId(StringReference);
}

void FArcNamedPrimaryAssetIdCustomization::UpdateThumbnail()
{
	if (AssetThumbnail.IsValid())
	{
		FAssetData AssetData;
		FPrimaryAssetId PrimaryAssetId = GetCurrentPrimaryAssetId();
		if (PrimaryAssetId.IsValid())
		{
			UAssetManager::Get().GetPrimaryAssetData(PrimaryAssetId, AssetData);
		}

		AssetThumbnail->SetAsset(AssetData);
	}
}

void FArcNamedPrimaryAssetIdCustomization::OnBrowseTo()
{
	FPrimaryAssetId PrimaryAssetId = GetCurrentPrimaryAssetId();

	if (PrimaryAssetId.IsValid())
	{
		FAssetData FoundData;
		if (UAssetManager::Get().GetPrimaryAssetData(PrimaryAssetId, FoundData))
		{
			TArray<FAssetData> SyncAssets;
			SyncAssets.Add(FoundData);
			GEditor->SyncBrowserToObjects(SyncAssets);
		}
	}	
}

void FArcNamedPrimaryAssetIdCustomization::OnUseSelected()
{
	TArray<FAssetData> SelectedAssets;
	GEditor->GetContentBrowserSelections(SelectedAssets);

	for (const FAssetData& AssetData : SelectedAssets)
	{
		FPrimaryAssetId PrimaryAssetId = UAssetManager::Get().GetPrimaryAssetIdForData(AssetData);
		if (PrimaryAssetId.IsValid())
		{
			OnIdSelected(PrimaryAssetId);
			return;
		}
	}
}

void FArcNamedPrimaryAssetIdCustomization::OnOpenAssetEditor()
{
	FAssetData AssetData;
	FPrimaryAssetId PrimaryAssetId = GetCurrentPrimaryAssetId();
	if (PrimaryAssetId.IsValid())
	{
		if (UAssetManager::Get().GetPrimaryAssetData(PrimaryAssetId, AssetData))
		{
			GEditor->EditObject(AssetData.GetAsset());
		}
	}
}

void FArcNamedPrimaryAssetIdCustomization::OnClear()
{
	OnIdSelected(FPrimaryAssetId());
}

FReply FArcNamedPrimaryAssetIdCustomization::OnAssetThumbnailDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	OnOpenAssetEditor();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE