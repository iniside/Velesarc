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
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SSearchableComboBox.h"
#include "DetailWidgetRow.h"
#include "PropertyHandle.h"

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

	if (HasSourcesForAllowedTypes())
	{
		HeaderRow
		.NameContent()
		[
			InStructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(250.0f)
		.MaxDesiredWidth(0.0f)
		[
			MakeSourceSelector(StructCustomizationUtils)
		];
		return;
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

bool FArcNamedPrimaryAssetIdCustomization::HasSourcesForAllowedTypes() const
{
	if (AllowedTypes.IsEmpty())
	{
		return false;
	}
	for (const FPrimaryAssetType& Type : AllowedTypes)
	{
		if (ArcPrimaryAssetIdSources::Find(Type) == nullptr)
		{
			return false;
		}
	}
	return true;
}

TSharedRef<SWidget> FArcNamedPrimaryAssetIdCustomization::MakeSourceSelector(IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SAssignNew(SourceComboBox, SSearchableComboBox)
			.OptionsSource(&SourceOptions)
			.OnComboBoxOpening(this, &FArcNamedPrimaryAssetIdCustomization::RefreshSourceOptions)
			.OnGenerateWidget(this, &FArcNamedPrimaryAssetIdCustomization::MakeSourceOptionWidget)
			.OnSelectionChanged(this, &FArcNamedPrimaryAssetIdCustomization::OnSourceSelectionChanged)
			.IsEnabled(this, &FArcNamedPrimaryAssetIdCustomization::IsSourceSelectorEditable)
			[
				SNew(STextBlock)
				.Text(this, &FArcNamedPrimaryAssetIdCustomization::GetSourceDisplayText)
				.ToolTipText(this, &FArcNamedPrimaryAssetIdCustomization::GetDisplayTooltipText)
				.Font(StructCustomizationUtils.GetRegularFont())
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2.0f, 0.0f)
		[
			SNew(SBox)
			.Visibility(this, &FArcNamedPrimaryAssetIdCustomization::GetSourceBrowseVisibility)
			[
				PropertyCustomizationHelpers::MakeBrowseButton(FSimpleDelegate::CreateSP(this, &FArcNamedPrimaryAssetIdCustomization::OnSourceBrowseTo),
					LOCTEXT("SourceBrowseToolTip", "Browse to the referenced asset"),
					TAttribute<bool>::CreateSP(this, &FArcNamedPrimaryAssetIdCustomization::CanSourceBrowse))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeClearButton(FSimpleDelegate::CreateSP(this, &FArcNamedPrimaryAssetIdCustomization::OnSourceClear),
				LOCTEXT("SourceClearToolTip", "Clear the reference"),
				TAttribute<bool>::CreateSP(this, &FArcNamedPrimaryAssetIdCustomization::IsSourceSelectorEditable))
		];
}

void FArcNamedPrimaryAssetIdCustomization::RefreshSourceOptions()
{
	SourceOptions.Reset();
	SourceOptionEntries.Reset();

	for (const FPrimaryAssetType& Type : AllowedTypes)
	{
		const FArcPrimaryAssetIdSource* Source = ArcPrimaryAssetIdSources::Find(Type);
		if (Source == nullptr)
		{
			continue;
		}
		TArray<FArcPrimaryAssetIdSourceEntry> Entries = Source->GetEntries.Execute();
		for (FArcPrimaryAssetIdSourceEntry& Entry : Entries)
		{
			if (Entry.Id.IsValid())
			{
				SourceOptionEntries.Add(MoveTemp(Entry));
			}
		}
	}

	SourceOptionEntries.Sort([](const FArcPrimaryAssetIdSourceEntry& A, const FArcPrimaryAssetIdSourceEntry& B)
	{
		const int32 Compare = A.DisplayName.Compare(B.DisplayName, ESearchCase::IgnoreCase);
		if (Compare != 0)
		{
			return Compare < 0;
		}
		return A.Id.LexicalLess(B.Id);
	});

	for (const FArcPrimaryAssetIdSourceEntry& Entry : SourceOptionEntries)
	{
		SourceOptions.Add(MakeShared<FString>(Entry.DisplayName.IsEmpty() ? Entry.Id.ToString() : Entry.DisplayName));
	}

	CachedDisplayId = FPrimaryAssetId();
	CachedDisplayName.Reset();

	if (SourceComboBox.IsValid())
	{
		SourceComboBox->RefreshOptions();
	}
}

TSharedRef<SWidget> FArcNamedPrimaryAssetIdCustomization::MakeSourceOptionWidget(TSharedPtr<FString> Option) const
{
	const int32 Index = SourceOptions.IndexOfByKey(Option);
	const FText TooltipText = SourceOptionEntries.IsValidIndex(Index)
		? FText::AsCultureInvariant(SourceOptionEntries[Index].Id.ToString())
		: FText::GetEmpty();

	return SNew(STextBlock)
		.Text(FText::AsCultureInvariant(Option.IsValid() ? *Option : FString()))
		.ToolTipText(TooltipText);
}

void FArcNamedPrimaryAssetIdCustomization::OnSourceSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	if (!Selection.IsValid() || SelectInfo == ESelectInfo::Direct)
	{
		return;
	}
	const int32 Index = SourceOptions.IndexOfByKey(Selection);
	if (!SourceOptionEntries.IsValidIndex(Index))
	{
		return;
	}
	const FArcPrimaryAssetIdSourceEntry Entry = SourceOptionEntries[Index];
	SetIdWithDisplayName(Entry.Id, Entry.DisplayName);
}

void FArcNamedPrimaryAssetIdCustomization::SetIdWithDisplayName(const FPrimaryAssetId& AssetId, const FString& DisplayName)
{
	if (!AssetIdPropertyHandle.IsValid() || !AssetIdPropertyHandle->IsValidHandle())
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("SetPrimaryAssetIdFromSource", "Set Primary Asset Id"));
	if (NamePropertyHandle.IsValid() && NamePropertyHandle->IsValidHandle())
	{
		NamePropertyHandle->SetValue(DisplayName);
	}
	AssetIdPropertyHandle->SetValueFromFormattedString(AssetId.ToString());

	CachedDisplayId = AssetId;
	CachedDisplayName = DisplayName;
}

FText FArcNamedPrimaryAssetIdCustomization::GetSourceDisplayText() const
{
	if (!AssetIdPropertyHandle.IsValid() || !AssetIdPropertyHandle->IsValidHandle())
	{
		return FText::GetEmpty();
	}

	FString IdString;
	const FPropertyAccess::Result Result = AssetIdPropertyHandle->GetValueAsFormattedString(IdString);
	if (Result == FPropertyAccess::MultipleValues)
	{
		return LOCTEXT("SourceMultipleValues", "Multiple Values");
	}

	const FPrimaryAssetId CurrentId(IdString);
	if (!CurrentId.IsValid())
	{
		return LOCTEXT("SourceNone", "None");
	}

	if (CurrentId != CachedDisplayId)
	{
		CachedDisplayId = CurrentId;
		CachedDisplayName.Reset();
		const FArcPrimaryAssetIdSource* Source = ArcPrimaryAssetIdSources::Find(CurrentId.PrimaryAssetType);
		if (Source != nullptr)
		{
			const TArray<FArcPrimaryAssetIdSourceEntry> Entries = Source->GetEntries.Execute();
			for (const FArcPrimaryAssetIdSourceEntry& Entry : Entries)
			{
				if (Entry.Id == CurrentId)
				{
					CachedDisplayName = Entry.DisplayName;
					break;
				}
			}
		}
	}

	return FText::AsCultureInvariant(CachedDisplayName.IsEmpty() ? IdString : CachedDisplayName);
}

bool FArcNamedPrimaryAssetIdCustomization::IsSourceSelectorEditable() const
{
	return StructPropertyHandle.IsValid() && StructPropertyHandle->IsEditable();
}

bool FArcNamedPrimaryAssetIdCustomization::CanSourceBrowse() const
{
	const FPrimaryAssetId CurrentId = GetCurrentPrimaryAssetId();
	if (!CurrentId.IsValid())
	{
		return false;
	}
	const FArcPrimaryAssetIdSource* Source = ArcPrimaryAssetIdSources::Find(CurrentId.PrimaryAssetType);
	return Source != nullptr && Source->BrowseTo.IsBound();
}

EVisibility FArcNamedPrimaryAssetIdCustomization::GetSourceBrowseVisibility() const
{
	for (const FPrimaryAssetType& Type : AllowedTypes)
	{
		const FArcPrimaryAssetIdSource* Source = ArcPrimaryAssetIdSources::Find(Type);
		if (Source != nullptr && Source->BrowseTo.IsBound())
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

void FArcNamedPrimaryAssetIdCustomization::OnSourceBrowseTo()
{
	const FPrimaryAssetId CurrentId = GetCurrentPrimaryAssetId();
	if (!CurrentId.IsValid())
	{
		return;
	}
	const FArcPrimaryAssetIdSource* Source = ArcPrimaryAssetIdSources::Find(CurrentId.PrimaryAssetType);
	if (Source != nullptr && Source->BrowseTo.IsBound())
	{
		const FArcBrowseToPrimaryAssetId BrowseTo = Source->BrowseTo;
		BrowseTo.Execute(CurrentId);
	}
}

void FArcNamedPrimaryAssetIdCustomization::OnSourceClear()
{
	SetIdWithDisplayName(FPrimaryAssetId(), FString());
}

FReply FArcNamedPrimaryAssetIdCustomization::OnAssetThumbnailDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	OnOpenAssetEditor();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE