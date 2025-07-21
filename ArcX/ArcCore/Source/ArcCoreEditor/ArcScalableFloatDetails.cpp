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

#include "ArcScalableFloatDetails.h"

#include "DataRegistryEditorModule.h"
#include "DataRegistrySubsystem.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "InstancedStructDetails.h"
#include "IPropertyUtilities.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Input/SSlider.h"
#include "SlateOptMacros.h"

#define LOCTEXT_NAMESPACE "AttributeDetailsCustomization"

TSharedRef<IPropertyTypeCustomization> FArcScalableFloatDetails::MakeInstance()
{
	return MakeShareable(new FArcScalableFloatDetails());
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FArcScalableFloatDetails::CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	bSourceRefreshQueued = false;
	PropertyUtilities = StructCustomizationUtils.GetPropertyUtilities();

	ValueProperty = StructPropertyHandle->GetChildHandle("Value");
	CurveTableHandleProperty = StructPropertyHandle->GetChildHandle("Curve");
	RegistryTypeProperty = StructPropertyHandle->GetChildHandle("RegistryType");

	if (ValueProperty.IsValid() && CurveTableHandleProperty.IsValid() && RegistryTypeProperty.IsValid())
	{
		RowNameProperty = CurveTableHandleProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FCurveTableRowHandle, RowName));
		CurveTableProperty = CurveTableHandleProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FCurveTableRowHandle, CurveTable));

		UpdatePreviewLevels();

		CurveTableProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FArcScalableFloatDetails::OnCurveSourceChanged));
		RegistryTypeProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FArcScalableFloatDetails::OnCurveSourceChanged));
		RowNameProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FArcScalableFloatDetails::OnRowNameChanged));

		TSharedPtr<IPropertyHandle> CustomScalingHandlePtr = StructPropertyHandle->GetChildHandle("CustomScaling");

		FDetailWidgetRow StructRow;
		TSharedRef<IPropertyTypeCustomization> Instance = FInstancedStructDetails::MakeInstance();
		Instance->CustomizeHeader(CustomScalingHandlePtr.ToSharedRef(), StructRow, StructCustomizationUtils);
		
		HeaderRow
			.NameContent()
			[
				StructPropertyHandle->CreatePropertyNameWidget()
			]
			.ValueContent()
			.MinDesiredWidth( 600 )
			.MaxDesiredWidth( 4096 )
			[
				SNew(SVerticalBox)
				.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FArcScalableFloatDetails::IsEditable)))

				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					.FillWidth(0.2f)
					//.AutoWidth()
					[
						SNew(STextBlock)
							.Text(this, &FArcScalableFloatDetails::GetRowValuePreviewText)
							.Justification(ETextJustify::Left)
					]
					+SHorizontalBox::Slot()
					.FillWidth(0.2f)
					.HAlign(HAlign_Fill)
					.Padding(1.f, 0.f, 2.f, 0.f)
					[
						ValueProperty->CreatePropertyValueWidget()
					]
		
					+SHorizontalBox::Slot()
					.FillWidth(0.40f)
					.HAlign(HAlign_Fill)
					.Padding(2.f, 0.f, 2.f, 0.f)
					[
						CreateCurveTableWidget()
					]

					+SHorizontalBox::Slot()
					.FillWidth(0.40f)
					.HAlign(HAlign_Fill)
					.Padding(2.f, 0.f, 0.f, 0.f)
					[
						CreateRegistryTypeWidget()
					]
					+SHorizontalBox::Slot()
					.FillWidth(0.40f)
					.HAlign(HAlign_Fill)
					.Padding(2.f, 0.f, 0.f, 0.f)
					[
						StructRow.ValueWidget.Widget
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					[
						SNew(SHorizontalBox)
						.Visibility(this, &FArcScalableFloatDetails::GetAssetButtonVisiblity)

						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							PropertyCustomizationHelpers::MakeUseSelectedButton(FSimpleDelegate::CreateSP(this, &FArcScalableFloatDetails::OnUseSelected))
						]
						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							PropertyCustomizationHelpers::MakeBrowseButton(FSimpleDelegate::CreateSP(this, &FArcScalableFloatDetails::OnBrowseTo))
						]
						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							PropertyCustomizationHelpers::MakeClearButton(FSimpleDelegate::CreateSP(this, &FArcScalableFloatDetails::OnClear))
						]
					]
					+SHorizontalBox::Slot()
					.FillWidth(0.4f)
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Center)
					[
						CreateRowNameWidget()
					]
				]

				//+ SVerticalBox::Slot()
				//.AutoHeight()
				//.Padding(0.f, 3.f, 0.f, 0.f)
				//[
				//	SNew(SHorizontalBox)
				//	.Visibility(this, &FArcScalableFloatDetails::GetRowNameVisibility)
				//	
				//	+SHorizontalBox::Slot()
				//	.FillWidth(0.4f)
				//	.HAlign(HAlign_Fill)
				//	.VAlign(VAlign_Center)
				//	[
				//		CreateRowNameWidget()
				//	]
				//
				//	+SHorizontalBox::Slot()
				//	.FillWidth(0.3f)
				//	.HAlign(HAlign_Fill)
				//	.Padding(2.f, 0.f, 2.f, 0.f)
				//	[
				//		SNew(SVerticalBox)
				//		.Visibility(this, &FArcScalableFloatDetails::GetPreviewVisibility)
				//
				//		+SVerticalBox::Slot()
				//		.HAlign(HAlign_Center)
				//		[
				//			SNew(STextBlock)
				//			.Text(this, &FArcScalableFloatDetails::GetRowValuePreviewLabel)
				//		]
				//
				//		+SVerticalBox::Slot()
				//		.HAlign(HAlign_Center)
				//		[
				//			SNew(STextBlock)
				//			.Text(this, &FArcScalableFloatDetails::GetRowValuePreviewText)
				//		]
				//	]
				//
				//	+SHorizontalBox::Slot()
				//	.FillWidth(0.3f)
				//	.HAlign(HAlign_Fill)
				//	.Padding(2.f, 0.f, 0.f, 0.f)
				//	[
				//		SNew(SSlider)
				//		.Visibility(this, &FArcScalableFloatDetails::GetPreviewVisibility)
				//		.ToolTipText(LOCTEXT("LevelPreviewToolTip", "Adjust the preview level."))
				//		.Value(this, &FArcScalableFloatDetails::GetPreviewLevel)
				//		.OnValueChanged(this, &FArcScalableFloatDetails::SetPreviewLevel)
				//	]
				//]
			];	
	}
}

TSharedRef<SWidget> FArcScalableFloatDetails::CreateCurveTableWidget()
{
	return SNew(SComboButton)
		.OnGetMenuContent(this, &FArcScalableFloatDetails::GetCurveTablePicker)
		.ContentPadding(FMargin(2.0f, 2.0f))
		.Visibility(this, &FArcScalableFloatDetails::GetCurveTableVisiblity)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(this, &FArcScalableFloatDetails::GetCurveTableText)
			.ToolTipText(this, &FArcScalableFloatDetails::GetCurveTableTooltip)
			.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		];

	// Need to make the buttons go away and show custom prompt
	return CurveTableProperty->CreatePropertyValueWidget(false);
}

TSharedRef<SWidget> FArcScalableFloatDetails::CreateRegistryTypeWidget()
{
	// Only support curve types
	static FName RealCurveName = FName("RealCurve");

	return SNew(SBox)
		.Padding(0.0f)
		.ToolTipText(this, &FArcScalableFloatDetails::GetRegistryTypeTooltip)
		.Visibility(this, &FArcScalableFloatDetails::GetRegistryTypeVisiblity)
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakePropertyComboBox(RegistryTypeProperty,
			FOnGetPropertyComboBoxStrings::CreateStatic(&FDataRegistryEditorModule::GenerateDataRegistryTypeComboBoxStrings, true, RealCurveName),
			FOnGetPropertyComboBoxValue::CreateSP(this, &FArcScalableFloatDetails::GetRegistryTypeValueString))
		];
}

TSharedRef<SWidget> FArcScalableFloatDetails::CreateRowNameWidget()
{
	FPropertyAccess::Result* OutResult = nullptr;

	return SNew(SBox)
		.Padding(0.0f)
		.ToolTipText(this, &FArcScalableFloatDetails::GetRowNameComboBoxContentTooltip)
		.VAlign(VAlign_Center)
		[
			FDataRegistryEditorModule::MakeDataRegistryItemNameSelector(
				FOnGetDataRegistryDisplayText::CreateSP(this, &FArcScalableFloatDetails::GetRowNameComboBoxContentText),
				FOnGetDataRegistryId::CreateSP(this, &FArcScalableFloatDetails::GetRegistryId, OutResult),
				FOnSetDataRegistryId::CreateSP(this, &FArcScalableFloatDetails::SetRegistryId),
				FOnGetCustomDataRegistryItemNames::CreateSP(this, &FArcScalableFloatDetails::GetCustomRowNames),
				true)
		];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<SWidget> FArcScalableFloatDetails::GetCurveTablePicker()
{
	const bool bAllowClear = true;
	TArray<const UClass*> AllowedClasses;
	AllowedClasses.Add(UCurveTable::StaticClass());

	FAssetData CurrentAssetData;
	UCurveTable* SelectedTable = GetCurveTable();

	if (SelectedTable)
	{
		CurrentAssetData = FAssetData(SelectedTable);
	}

	return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(CurrentAssetData,
		bAllowClear,
		AllowedClasses,
		PropertyCustomizationHelpers::GetNewAssetFactoriesForClasses(AllowedClasses),
		FOnShouldFilterAsset(),
		FOnAssetSelected::CreateSP(this, &FArcScalableFloatDetails::OnSelectCurveTable),
		FSimpleDelegate::CreateSP(this, &FArcScalableFloatDetails::OnCloseMenu));
}

void FArcScalableFloatDetails::OnSelectCurveTable(const FAssetData& AssetData)
{
	UObject* SelectedTable = AssetData.GetAsset();

	CurveTableProperty->SetValue(SelectedTable);
	
	// Also clear type
	RegistryTypeProperty->SetValueFromFormattedString(FString());
}

void FArcScalableFloatDetails::OnCloseMenu()
{
	FSlateApplication::Get().DismissAllMenus();
}

FText FArcScalableFloatDetails::GetCurveTableText() const
{
	FPropertyAccess::Result FoundResult;
	UCurveTable* SelectedTable = GetCurveTable(&FoundResult);

	if (SelectedTable)
	{
		return FText::AsCultureInvariant(SelectedTable->GetName());
	}
	else if (FoundResult == FPropertyAccess::MultipleValues)
	{
		return LOCTEXT("MultipleValues", "Multiple Values");
	}

	return LOCTEXT("PickCurveTable", "Use CurveTable...");
}

FText FArcScalableFloatDetails::GetCurveTableTooltip() const
{
	UCurveTable* SelectedTable = GetCurveTable();

	if (SelectedTable)
	{
		return FText::AsCultureInvariant(SelectedTable->GetPathName());
	}

	return LOCTEXT("PickCurveTableTooltip", "Select a CurveTable asset containing Curve to multiply by Value");
}

EVisibility FArcScalableFloatDetails::GetCurveTableVisiblity() const
{
	FDataRegistryType RegistryType = GetRegistryType();
	return RegistryType.IsValid() ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility FArcScalableFloatDetails::GetAssetButtonVisiblity() const
{
	UCurveTable* CurveTable = GetCurveTable();
	return CurveTable ? EVisibility::Visible : EVisibility::Collapsed;
}

void FArcScalableFloatDetails::OnBrowseTo()
{
	UCurveTable* CurveTable = GetCurveTable();
	if (CurveTable)
	{
		TArray<FAssetData> SyncAssets;
		SyncAssets.Add(FAssetData(CurveTable));
		GEditor->SyncBrowserToObjects(SyncAssets);
	}
}

void FArcScalableFloatDetails::OnClear()
{
	OnSelectCurveTable(FAssetData());
}

void FArcScalableFloatDetails::OnUseSelected()
{
	TArray<FAssetData> SelectedAssets;
	GEditor->GetContentBrowserSelections(SelectedAssets);

	for (const FAssetData& AssetData : SelectedAssets)
	{
		UCurveTable* FoundTable = Cast<UCurveTable>(AssetData.GetAsset());

		if (FoundTable)
		{
			OnSelectCurveTable(AssetData);
			return;
		}
	}
}

FString FArcScalableFloatDetails::GetRegistryTypeValueString() const
{
	FPropertyAccess::Result FoundResult;
	FDataRegistryType RegistryType = GetRegistryType(&FoundResult);

	if (RegistryType.IsValid())
	{
		return RegistryType.ToString();
	}
	else if (FoundResult == FPropertyAccess::MultipleValues)
	{
		return LOCTEXT("MultipleValues", "Multiple Values").ToString();
	}

	return LOCTEXT("PickRegistry", "Use Registry...").ToString();
}

FText FArcScalableFloatDetails::GetRegistryTypeTooltip() const
{
	return LOCTEXT("PickRegistryTooltip", "Select a DataRegistry containing Curve to multiply by Value");
}

EVisibility FArcScalableFloatDetails::GetRegistryTypeVisiblity() const
{
	UCurveTable* CurveTable = GetCurveTable();
	FDataRegistryType RegistryType = GetRegistryType();
	bool bIsSystemEnabled = UDataRegistrySubsystem::Get()->IsConfigEnabled();
	return RegistryType.IsValid() || (bIsSystemEnabled && !CurveTable) ? EVisibility::Visible : EVisibility::Collapsed;
}

void FArcScalableFloatDetails::OnCurveSourceChanged()
{
	// Need a frame deferral to deal with multi edit caching problems
	TSharedPtr<IPropertyUtilities> PinnedUtilities = PropertyUtilities.Pin();
	if (!bSourceRefreshQueued && PinnedUtilities.IsValid())
	{
		PinnedUtilities->EnqueueDeferredAction(FSimpleDelegate::CreateSP(this, &FArcScalableFloatDetails::RefreshSourceData));
		bSourceRefreshQueued = true;
	}
}

void FArcScalableFloatDetails::OnRowNameChanged()
{
	UpdatePreviewLevels();
}

void FArcScalableFloatDetails::RefreshSourceData()
{
	// Set the default value to 1.0 when using a curve source, so the value in the table is used directly. Only do this if the value is currently 0 (default)
	// Set it back to 0 when setting back. Only do this if the value is currently 1 to go back to the default.

	UObject* CurveTable = GetCurveTable();
	FDataRegistryType RegistryType = GetRegistryType();

	float Value = -1.0f;
	FPropertyAccess::Result ValueResult = ValueProperty->GetValue(Value);

	// Only modify if all are the same for multi select
	if (CurveTable || RegistryType.IsValid())
	{
		if (Value == 0.f || ValueResult == FPropertyAccess::MultipleValues)
		{
			ValueProperty->SetValue(1.f);
		}
	}
	else
	{
		if (Value == 1.f || ValueResult == FPropertyAccess::MultipleValues)
		{
			ValueProperty->SetValue(0.f);
		}
	}

	if (RegistryType.IsValid())
	{
		// Registry type has priority over curve table
		UCurveTable* NullTable = nullptr;
		CurveTableProperty->SetValue(NullTable);
	}

	bSourceRefreshQueued = false;
}

UCurveTable* FArcScalableFloatDetails::GetCurveTable(FPropertyAccess::Result* OutResult) const
{
	FPropertyAccess::Result TempResult;
	if (OutResult == nullptr)
	{
		OutResult = &TempResult;
	}

	UCurveTable* CurveTable = nullptr;
	if (CurveTableProperty.IsValid())
	{
		*OutResult = CurveTableProperty->GetValue((UObject*&)CurveTable);
	}
	else
	{
		*OutResult = FPropertyAccess::Fail;
	}

	return CurveTable;
}

FDataRegistryType FArcScalableFloatDetails::GetRegistryType(FPropertyAccess::Result* OutResult) const
{
	FPropertyAccess::Result TempResult;
	if (OutResult == nullptr)
	{
		OutResult = &TempResult;
	}
	
	FString RegistryString;

	// Bypassing the struct because GetValueAsFormattedStrings doesn't work well on multi select
	if (RegistryTypeProperty.IsValid())
	{
		*OutResult = RegistryTypeProperty->GetValueAsFormattedString(RegistryString);
	}
	else
	{
		*OutResult = FPropertyAccess::Fail;
	}

	return FDataRegistryType(*RegistryString);
}

EVisibility FArcScalableFloatDetails::GetRowNameVisibility() const
{
	UCurveTable* CurveTable = GetCurveTable();
	FDataRegistryType RegistryType = GetRegistryType();

	return (CurveTable || RegistryType.IsValid()) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FArcScalableFloatDetails::GetPreviewVisibility() const
{
	FName CurrentRow = GetRowName();
	return (CurrentRow != NAME_None) ? EVisibility::Visible : EVisibility::Hidden;
}

float FArcScalableFloatDetails::GetPreviewLevel() const
{
	return (MaxPreviewLevel != MinPreviewLevel) ? (PreviewLevel-MinPreviewLevel) / (MaxPreviewLevel-MinPreviewLevel) : 0;
}

void FArcScalableFloatDetails::SetPreviewLevel(float NewLevel)
{
	PreviewLevel = FMath::FloorToInt(NewLevel * (MaxPreviewLevel - MinPreviewLevel) + MinPreviewLevel);
}

FText FArcScalableFloatDetails::GetRowNameComboBoxContentText() const
{
	FPropertyAccess::Result RowResult;
	FName RowName = GetRowName(&RowResult);

	if (RowResult != FPropertyAccess::MultipleValues)
	{
		if (RowName != NAME_None)
		{
			return FText::FromName(RowName);
		}
		else
		{
			return LOCTEXT("SelectCurve", "Select Curve...");
		}
	}
	return LOCTEXT("MultipleValues", "Multiple Values");
}

FText FArcScalableFloatDetails::GetRowNameComboBoxContentTooltip() const
{
	FName RowName = GetRowName();

	if (RowName != NAME_None)
	{
		return FText::FromName(RowName);
	}

	return LOCTEXT("SelectCurveTooltip", "Select a Curve, this will be scaled using input level and then multiplied by Value");
}

FText FArcScalableFloatDetails::GetRowValuePreviewLabel() const
{
	FPropertyAccess::Result FoundResult;
	const FRealCurve* FoundCurve = GetRealCurve(&FoundResult);
	if (FoundCurve)
	{
		return FText::Format(LOCTEXT("LevelPreviewLabel", "Preview At {0}"), FText::AsNumber(PreviewLevel));
	}
	else if (FoundResult == FPropertyAccess::MultipleValues)
	{
		return LOCTEXT("MultipleValues", "Multiple Values");
	}
	else
	{
		return LOCTEXT("ErrorFindingCurve", "ERROR: Invalid Curve!");
	}
}

FText FArcScalableFloatDetails::GetRowValuePreviewText() const
{
	const FRealCurve* FoundCurve = GetRealCurve();
	if (FoundCurve)
	{
		float Value;
		if (ValueProperty->GetValue(Value) == FPropertyAccess::Success)
		{
			static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
				.SetMinimumFractionalDigits(3)
				.SetMaximumFractionalDigits(3);
			return FText::AsNumber(Value * FoundCurve->Eval(PreviewLevel), &FormatOptions);
		}
	}

	return FText::GetEmpty();
}

FName FArcScalableFloatDetails::GetRowName(FPropertyAccess::Result* OutResult) const
{
	FPropertyAccess::Result TempResult;
	if (OutResult == nullptr)
	{
		OutResult = &TempResult;
	}

	FName ReturnName;
	if (RowNameProperty.IsValid())
	{
		*OutResult = RowNameProperty->GetValue(ReturnName);
	}
	else
	{
		*OutResult = FPropertyAccess::Fail;
	}

	return ReturnName;
}

FDataRegistryId FArcScalableFloatDetails::GetRegistryId(FPropertyAccess::Result* OutResult) const
{
	FPropertyAccess::Result TempResult;
	if (OutResult == nullptr)
	{
		OutResult = &TempResult;
	}

	// Cache name result so we can return multiple values
	FPropertyAccess::Result NameResult;
	FName RowName = GetRowName(&NameResult);

	UCurveTable* CurveTable = GetCurveTable(OutResult);
	if (CurveTable)
	{
		// Curve tables are all valid but names may differ
		*OutResult = NameResult;

		// Use the fake custom type, options will get filled in by GetCustomRowNames
		return FDataRegistryId(FDataRegistryType::CustomContextType, RowName);
	}

	// This is a real registry, or is empty/invalid
	FDataRegistryType RegistryType = GetRegistryType(OutResult);

	if (*OutResult == FPropertyAccess::Success)
	{
		// Names may differ
		*OutResult = NameResult;
	}

	return FDataRegistryId(RegistryType, RowName);
}

void FArcScalableFloatDetails::SetRegistryId(FDataRegistryId NewId)
{
	// Always set row name, only set type if it's valid and different
	RowNameProperty->SetValue(NewId.ItemName);

	FDataRegistryType CurrentType = GetRegistryType();
	if (NewId.RegistryType != FDataRegistryType::CustomContextType && NewId.RegistryType != CurrentType)
	{
		RegistryTypeProperty->SetValueFromFormattedString(NewId.RegistryType.ToString());
	}
}

void FArcScalableFloatDetails::GetCustomRowNames(TArray<FName>& OutRows) const
{
	UCurveTable* CurveTable = GetCurveTable();

	if (CurveTable != nullptr)
	{
		for (TMap<FName, FRealCurve*>::TConstIterator Iterator(CurveTable->GetRowMap()); Iterator; ++Iterator)
		{
			OutRows.Add(Iterator.Key());
		}
	}
}

const FRealCurve* FArcScalableFloatDetails::GetRealCurve(FPropertyAccess::Result* OutResult) const
{
	FPropertyAccess::Result TempResult;
	if (OutResult == nullptr)
	{
		OutResult = &TempResult;
	}

	// First check curve table, abort if values differ
	UCurveTable* CurveTable = GetCurveTable(OutResult);
	if (*OutResult != FPropertyAccess::Success)
	{
		return nullptr;
	}

	FName RowName = GetRowName(OutResult);
	if (*OutResult != FPropertyAccess::Success)
	{
		return nullptr;
	}

	if (CurveTable && !RowName.IsNone())
	{
		return CurveTable->FindCurveUnchecked(RowName);
	}

	FDataRegistryId RegistryId = GetRegistryId(OutResult);
	if (RegistryId.IsValid())
	{
		// Now try registry, we will only get here if there are not multiple values
		UDataRegistry* Registry = UDataRegistrySubsystem::Get()->GetRegistryForType(RegistryId.RegistryType);
		if (Registry)
		{
			const FRealCurve* OutCurve = nullptr;
			if (Registry->GetCachedCurveRaw(OutCurve, RegistryId))
			{
				return OutCurve;
			}
		}
	}

	return nullptr;
}

bool FArcScalableFloatDetails::IsEditable() const
{
	return true;
}

void FArcScalableFloatDetails::UpdatePreviewLevels()
{
	if (const FRealCurve* FoundCurve = GetRealCurve())
	{
		FoundCurve->GetTimeRange(MinPreviewLevel, MaxPreviewLevel);
	}
	else
	{
		MinPreviewLevel = DefaultMinPreviewLevel;
		MaxPreviewLevel = DefaultMaxPreviewLevel;
	}

	PreviewLevel = FMath::Clamp(PreviewLevel, MinPreviewLevel, MaxPreviewLevel);
}

void FArcScalableFloatDetails::CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	TSharedPtr<IPropertyHandle> CustomScalingHandlePtr = StructPropertyHandle->GetChildHandle("CustomScaling");
	StructBuilder.AddProperty(CustomScalingHandlePtr.ToSharedRef());
}

//-------------------------------------------------------------------------------------

#undef LOCTEXT_NAMESPACE