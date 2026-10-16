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
#include "ArcScalableFloat.h"

#include "Curves/RichCurve.h"
#include "DataRegistryEditorModule.h"
#include "SCurveEditor.h"
#include "DataRegistrySubsystem.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "InstancedStructDetails.h"
#include "IPropertyUtilities.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Input/SSlider.h"
#include "SlateOptMacros.h"
#include "Styling/CoreStyle.h"
#include "Engine/CurveTable.h"
#include "Engine/DataTable.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "AttributeDetailsCustomization"

TSharedRef<IPropertyTypeCustomization> FArcScalableFloatDetails::MakeInstance()
{
	return MakeShareable(new FArcScalableFloatDetails());
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FArcScalableFloatDetails::CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	bSourceRefreshQueued = false;
	bSourceModeResetQueued = false;
	PropertyUtilities = StructCustomizationUtils.GetPropertyUtilities();

	ValueProperty = StructPropertyHandle->GetChildHandle("Value");
	CurveTableHandleProperty = StructPropertyHandle->GetChildHandle("Curve");
	RegistryTypeProperty = StructPropertyHandle->GetChildHandle("RegistryType");
	SourceProperty = StructPropertyHandle->GetChildHandle("Source");

	// Range-source child handles
	MinValueProperty = StructPropertyHandle->GetChildHandle("MinValue");
	MaxValueProperty = StructPropertyHandle->GetChildHandle("MaxValue");
	DataTableProperty = StructPropertyHandle->GetChildHandle("DataTable");
	DataTableRowNameProperty = DataTableProperty.IsValid() ? DataTableProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDataTableRowHandle, RowName)) : nullptr;
	RangeRowNameProperty = StructPropertyHandle->GetChildHandle("RangeRowName");

	// Inline curve handle
	InlineCurveProperty = StructPropertyHandle->GetChildHandle("InlineCurve");

	if (ValueProperty.IsValid() && CurveTableHandleProperty.IsValid() && RegistryTypeProperty.IsValid())
	{
		RowNameProperty = CurveTableHandleProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FCurveTableRowHandle, RowName));
		CurveTableProperty = CurveTableHandleProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FCurveTableRowHandle, CurveTable));

		UpdatePreviewLevels();

		CurveTableProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FArcScalableFloatDetails::OnCurveSourceChanged));
		RegistryTypeProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FArcScalableFloatDetails::OnCurveSourceChanged));
		RowNameProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FArcScalableFloatDetails::OnRowNameChanged));

		if (SourceProperty.IsValid())
		{
			SourceProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FArcScalableFloatDetails::OnSourceModeChanged));
		}

		CustomScalingProperty = StructPropertyHandle->GetChildHandle("CustomScaling");

		FDetailWidgetRow StructRow;
		InstancedStructCustomization = FInstancedStructDetails::MakeInstance();
		InstancedStructCustomization->CustomizeHeader(CustomScalingProperty.ToSharedRef(), StructRow, StructCustomizationUtils);

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
					.AutoWidth()
					.Padding(0.f, 0.f, 4.f, 0.f)
					[
						SNew(STextBlock)
							.Text(this, &FArcScalableFloatDetails::GetRowValuePreviewText)
							.Justification(ETextJustify::Left)
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
					]
					+SHorizontalBox::Slot()
					.FillWidth(0.2f)
					.HAlign(HAlign_Fill)
					.Padding(1.f, 0.f, 2.f, 0.f)
					[
						ValueProperty->CreatePropertyValueWidget()
					]

					// Source combo — explicit single source of truth
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.f, 0.f, 4.f, 0.f)
					[
						SourceProperty->CreatePropertyValueWidget()
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

					// Inline curve editor (visible only for InlineCurve source)
					+SHorizontalBox::Slot()
					.FillWidth(0.6f)
					.HAlign(HAlign_Fill)
					.Padding(2.f, 0.f, 2.f, 0.f)
					[
						CreateInlineCurveWidget()
					]

					// Range widget — Min/Max entry + optional DataTable picker
					+SHorizontalBox::Slot()
					.FillWidth(0.6f)
					.HAlign(HAlign_Fill)
					.Padding(2.f, 0.f, 2.f, 0.f)
					[
						CreateRangeWidget()
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Fill)
					.Padding(2.f, 0.f, 0.f, 0.f)
					[
						SNew(SBox)
						.Visibility(this, &FArcScalableFloatDetails::GetCustomScalingVisibility)
						[
							StructRow.ValueWidget.Widget
						]
					]
				]
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
		.Visibility(this, &FArcScalableFloatDetails::GetRowNameVisibility)
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

TSharedRef<SWidget> FArcScalableFloatDetails::CreateRangeWidget()
{
	// Vertical stack: [Min | Max] row, then optional DataTable picker row
	return SNew(SBox)
		.Visibility(this, &FArcScalableFloatDetails::GetRangeWidgetVisibility)
		[
			SNew(SVerticalBox)

			// Inline Min/Max fields (visible for InlineRange; also shown for DataTableRange/DataRegistryRange
			// as informational labels when no inline data is relevant, but hidden via inner visibility)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				.Visibility(this, &FArcScalableFloatDetails::GetInlineRangeVisibility)

				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 2.f, 0.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("RangeMin", "Min"))
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
				+SHorizontalBox::Slot()
				.FillWidth(0.45f)
				.HAlign(HAlign_Fill)
				.Padding(0.f, 0.f, 4.f, 0.f)
				[
					MinValueProperty.IsValid()
						? MinValueProperty->CreatePropertyValueWidget()
						: SNullWidget::NullWidget
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 2.f, 0.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("RangeMax", "Max"))
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
				+SHorizontalBox::Slot()
				.FillWidth(0.45f)
				.HAlign(HAlign_Fill)
				[
					MaxValueProperty.IsValid()
						? MaxValueProperty->CreatePropertyValueWidget()
						: SNullWidget::NullWidget
				]
			]

			// DataTableRange: asset picker + row picker on one line (visible for DataTableRange only)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				.Visibility(this, &FArcScalableFloatDetails::GetDataTableRangePickerVisibility)

				+SHorizontalBox::Slot()
				.FillWidth(0.5f)
				.HAlign(HAlign_Fill)
				.Padding(0.f, 0.f, 2.f, 0.f)
				[
					SNew(SComboButton)
					.OnGetMenuContent(this, &FArcScalableFloatDetails::GetDataTableRangePicker)
					.ContentPadding(FMargin(2.0f, 2.0f))
					.ButtonContent()
					[
						SNew(STextBlock)
						.Text(this, &FArcScalableFloatDetails::GetDataTableRangeText)
						.ToolTipText(this, &FArcScalableFloatDetails::GetDataTableRangeTooltip)
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					]
				]
				+SHorizontalBox::Slot()
				.FillWidth(0.5f)
				.HAlign(HAlign_Fill)
				[
					CreateDataTableRangeRowWidget()
				]
			]

			// DataRegistryRange: registry type combo + row selector on one line (visible for DataRegistryRange only)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				.Visibility(this, &FArcScalableFloatDetails::GetDataRegistryRangeVisibility)

				+SHorizontalBox::Slot()
				.FillWidth(0.5f)
				.HAlign(HAlign_Fill)
				.Padding(0.f, 0.f, 2.f, 0.f)
				[
					CreateRangeRegistryTypeWidget()
				]
				+SHorizontalBox::Slot()
				.FillWidth(0.5f)
				.HAlign(HAlign_Fill)
				[
					CreateRangeRegistryRowWidget()
				]
			]
		];
}

TSharedRef<SWidget> FArcScalableFloatDetails::CreateRangeRegistryTypeWidget()
{
	// Filter registries to those whose item struct is FArcScalableFloatRange.
	static FName RangeStructName = FName("ArcScalableFloatRange");

	return SNew(SBox)
		.Padding(0.0f)
		.ToolTipText(LOCTEXT("PickRangeRegistryTooltip", "Select a DataRegistry whose item struct is FArcScalableFloatRange (Min/Max)"))
		.Visibility(this, &FArcScalableFloatDetails::GetDataRegistryRangeVisibility)
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakePropertyComboBox(RegistryTypeProperty,
			FOnGetPropertyComboBoxStrings::CreateStatic(&FDataRegistryEditorModule::GenerateDataRegistryTypeComboBoxStrings, true, RangeStructName),
			FOnGetPropertyComboBoxValue::CreateSP(this, &FArcScalableFloatDetails::GetRegistryTypeValueString))
		];
}

TSharedRef<SWidget> FArcScalableFloatDetails::CreateRangeRegistryRowWidget()
{
	FPropertyAccess::Result* OutResult = nullptr;

	return SNew(SBox)
		.Padding(0.0f)
		.ToolTipText(LOCTEXT("PickRangeRegistryRowTooltip", "Select the FArcScalableFloatRange row to roll Min..Max from"))
		.Visibility(this, &FArcScalableFloatDetails::GetDataRegistryRangeVisibility)
		.VAlign(VAlign_Center)
		[
			FDataRegistryEditorModule::MakeDataRegistryItemNameSelector(
				FOnGetDataRegistryDisplayText::CreateSP(this, &FArcScalableFloatDetails::GetRangeRegistryRowText),
				FOnGetDataRegistryId::CreateSP(this, &FArcScalableFloatDetails::GetRangeRegistryId, OutResult),
				FOnSetDataRegistryId::CreateSP(this, &FArcScalableFloatDetails::SetRangeRegistryId),
				FOnGetCustomDataRegistryItemNames(),
				true)
		];
}

TSharedRef<SWidget> FArcScalableFloatDetails::CreateDataTableRangeRowWidget()
{
	if (!DataTableRowNameProperty.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	return SNew(SBox)
		.Visibility(this, &FArcScalableFloatDetails::GetDataTableRangePickerVisibility)
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakePropertyComboBox(
				DataTableRowNameProperty,
				FOnGetPropertyComboBoxStrings::CreateSP(this, &FArcScalableFloatDetails::GetDataTableRangeRowStrings),
				FOnGetPropertyComboBoxValue::CreateSP(this, &FArcScalableFloatDetails::GetDataTableRangeRowValueString))
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
	return GetSourceValue() == static_cast<uint8>(EArcScalableFloatSource::CurveTableRow)
		? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FArcScalableFloatDetails::GetAssetButtonVisiblity() const
{
	if (GetSourceValue() != static_cast<uint8>(EArcScalableFloatSource::CurveTableRow))
	{
		return EVisibility::Collapsed;
	}
	UCurveTable* CurveTable = GetCurveTable();
	return CurveTable != nullptr ? EVisibility::Visible : EVisibility::Collapsed;
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
	return GetSourceValue() == static_cast<uint8>(EArcScalableFloatSource::DataRegistryRow)
		? EVisibility::Visible : EVisibility::Collapsed;
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
	// Range sources own their own Source value; the curve/registry change path must not stomp it.
	if (IsRangeSource())
	{
		bSourceRefreshQueued = false;
		return;
	}

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

void FArcScalableFloatDetails::OnSourceModeChanged()
{
	TSharedPtr<IPropertyUtilities> PinnedUtilities = PropertyUtilities.Pin();
	if (!bSourceModeResetQueued && PinnedUtilities.IsValid())
	{
		PinnedUtilities->EnqueueDeferredAction(FSimpleDelegate::CreateSP(this, &FArcScalableFloatDetails::ApplySourceModeReset));
		bSourceModeResetQueued = true;
	}
}

void FArcScalableFloatDetails::ApplySourceModeReset()
{
	const EArcScalableFloatSource Source = static_cast<EArcScalableFloatSource>(GetSourceValue());

	// Determine which handles to keep, clear the rest.
	// bKeep* == true means the field is USED by this source, so leave it alone.
	const bool bKeepCurveTable      = (Source == EArcScalableFloatSource::CurveTableRow);
	// DataRegistryRange deliberately does NOT keep RegistryType: its registry filter is
	// FArcScalableFloatRange, not RealCurve, so a type carried over from DataRegistryRow is invalid here.
	const bool bKeepRegistryType    = (Source == EArcScalableFloatSource::DataRegistryRow);
	const bool bKeepCurveRowName    = (Source == EArcScalableFloatSource::CurveTableRow || Source == EArcScalableFloatSource::DataRegistryRow);
	const bool bKeepDataTable       = (Source == EArcScalableFloatSource::DataTableRange);
	const bool bKeepRangeRowName    = (Source == EArcScalableFloatSource::DataRegistryRange);
	const bool bKeepMinMax          = (Source == EArcScalableFloatSource::InlineRange);

	if (!bKeepCurveTable && CurveTableProperty.IsValid())
	{
		CurveTableProperty->SetValue((UObject*)nullptr);
	}

	if (!bKeepRegistryType && RegistryTypeProperty.IsValid())
	{
		RegistryTypeProperty->SetValueFromFormattedString(FString());
	}

	if (!bKeepCurveRowName && RowNameProperty.IsValid())
	{
		RowNameProperty->SetValue(NAME_None);
	}

	if (!bKeepDataTable && DataTableProperty.IsValid())
	{
		TSharedPtr<IPropertyHandle> InnerDataTableHandle = DataTableProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDataTableRowHandle, DataTable));
		if (InnerDataTableHandle.IsValid())
		{
			InnerDataTableHandle->SetValue((UObject*)nullptr);
		}

		if (DataTableRowNameProperty.IsValid())
		{
			DataTableRowNameProperty->SetValue(NAME_None);
		}
	}

	if (!bKeepRangeRowName && RangeRowNameProperty.IsValid())
	{
		RangeRowNameProperty->SetValue(NAME_None);
	}

	if (!bKeepMinMax)
	{
		if (MinValueProperty.IsValid())
		{
			MinValueProperty->SetValue(0.f);
		}
		if (MaxValueProperty.IsValid())
		{
			MaxValueProperty->SetValue(0.f);
		}
	}

	// Value nicety: when switching to a curve-backed source, default Value 0 -> 1
	// so the curve drives the output directly. Range sources use Value as a multiplier
	// on the rolled [Min,Max], so they get the same 0 -> 1 default.
	if (Source == EArcScalableFloatSource::CurveTableRow
		|| Source == EArcScalableFloatSource::DataRegistryRow
		|| Source == EArcScalableFloatSource::InlineCurve
		|| Source == EArcScalableFloatSource::InlineRange
		|| Source == EArcScalableFloatSource::DataTableRange
		|| Source == EArcScalableFloatSource::DataRegistryRange)
	{
		if (ValueProperty.IsValid())
		{
			float CurrentValue = 0.f;
			if (ValueProperty->GetValue(CurrentValue) == FPropertyAccess::Success && CurrentValue == 0.f)
			{
				ValueProperty->SetValue(1.f);
			}
		}
	}

	bSourceModeResetQueued = false;

	TSharedPtr<IPropertyUtilities> PinnedUtilities = PropertyUtilities.Pin();
	if (PinnedUtilities.IsValid())
	{
		PinnedUtilities->ForceRefresh();
	}
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
	const uint8 Source = GetSourceValue();
	return (Source == static_cast<uint8>(EArcScalableFloatSource::CurveTableRow)
		|| Source == static_cast<uint8>(EArcScalableFloatSource::DataRegistryRow))
		? EVisibility::Visible : EVisibility::Collapsed;
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
			if (GetSourceValue() == static_cast<uint8>(EArcScalableFloatSource::DataRegistryRow))
			{
				return LOCTEXT("SelectRow", "Select Row...");
			}
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
	// For range sources: show "Min – Max" from property handles, never evaluate/roll.
	const uint8 CurrentSource = GetSourceValue();
	const EArcScalableFloatSource Source = static_cast<EArcScalableFloatSource>(CurrentSource);

	static const FNumberFormattingOptions RangeFormat = FNumberFormattingOptions()
		.SetMinimumFractionalDigits(2)
		.SetMaximumFractionalDigits(2);

	// Value multiplies the resolved range bounds at runtime (FArcScalableFloat::GetRangeMultiplier).
	// Value==0 is treated as 1 so the preview matches the roll for legacy assets.
	float RangeMult = 1.f;
	if (ValueProperty.IsValid())
	{
		float RawValue = 0.f;
		if (ValueProperty->GetValue(RawValue) == FPropertyAccess::Success && RawValue != 0.f)
		{
			RangeMult = RawValue;
		}
	}

	if (Source == EArcScalableFloatSource::InlineRange)
	{
		float MinVal = 0.f;
		float MaxVal = 0.f;
		if (MinValueProperty.IsValid() && MaxValueProperty.IsValid())
		{
			MinValueProperty->GetValue(MinVal);
			MaxValueProperty->GetValue(MaxVal);
		}

		return FText::Format(
			LOCTEXT("InlineRangePreview", "{0} – {1}"),
			FText::AsNumber(MinVal * RangeMult, &RangeFormat),
			FText::AsNumber(MaxVal * RangeMult, &RangeFormat));
	}

	if (Source == EArcScalableFloatSource::DataTableRange)
	{
		FName RowName;
		UDataTable* DataTable = nullptr;
		if (DataTableProperty.IsValid())
		{
			TSharedPtr<IPropertyHandle> DataTableRowNameHandle = DataTableProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDataTableRowHandle, RowName));
			if (DataTableRowNameHandle.IsValid())
			{
				DataTableRowNameHandle->GetValue(RowName);
			}

			TSharedPtr<IPropertyHandle> InnerDataTableHandle = DataTableProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDataTableRowHandle, DataTable));
			if (InnerDataTableHandle.IsValid())
			{
				UObject* DataTableObj = nullptr;
				InnerDataTableHandle->GetValue(DataTableObj);
				DataTable = Cast<UDataTable>(DataTableObj);
			}
		}

		if (RowName.IsNone())
		{
			return LOCTEXT("DataTableRangeNoRow", "Pick row...");
		}

		if (DataTable)
		{
			const FArcScalableFloatRange* Row = DataTable->FindRow<FArcScalableFloatRange>(RowName, TEXT("ArcScalableFloatDetails Preview"), /*bWarnIfRowMissing=*/false);
			if (Row)
			{
				// Row name is already shown in the picker — preview just shows the resolved values.
				return FText::Format(
					LOCTEXT("DataTableRangeValuePreview", "{0} – {1}"),
					FText::AsNumber(Row->Min * RangeMult, &RangeFormat),
					FText::AsNumber(Row->Max * RangeMult, &RangeFormat));
			}
		}

		return FText::GetEmpty();
	}

	if (Source == EArcScalableFloatSource::DataRegistryRange)
	{
		FName RowName;
		if (RangeRowNameProperty.IsValid())
		{
			RangeRowNameProperty->GetValue(RowName);
		}

		if (RowName.IsNone())
		{
			return LOCTEXT("DataRegistryRangeNoRow", "Pick registry row...");
		}

		FDataRegistryId RegistryId = GetRangeRegistryId();
		if (RegistryId.IsValid())
		{
			if (UDataRegistrySubsystem* SubSystem = UDataRegistrySubsystem::Get())
			{
				const FArcScalableFloatRange* Item = SubSystem->GetCachedItem<FArcScalableFloatRange>(RegistryId);
				if (Item)
				{
					// Row name is already shown in the picker — preview just shows the resolved values.
					return FText::Format(
						LOCTEXT("DataRegistryRangeValuePreview", "{0} – {1}"),
						FText::AsNumber(Item->Min * RangeMult, &RangeFormat),
						FText::AsNumber(Item->Max * RangeMult, &RangeFormat));
				}
			}
		}

		return FText::GetEmpty();
	}

	// Curve path: evaluate using the resolved curve (never rolls)
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
	if (InstancedStructCustomization.IsValid() && CustomScalingProperty.IsValid())
	{
		InstancedStructCustomization->CustomizeChildren(CustomScalingProperty.ToSharedRef(), StructBuilder, StructCustomizationUtils);
	}
}

//-------------------------------------------------------------------------------------
// Range source helpers

uint8 FArcScalableFloatDetails::GetSourceValue() const
{
	uint8 Value = static_cast<uint8>(EArcScalableFloatSource::None);
	if (SourceProperty.IsValid())
	{
		SourceProperty->GetValue(Value);
	}
	return Value;
}

bool FArcScalableFloatDetails::IsRangeSource() const
{
	const EArcScalableFloatSource Source = static_cast<EArcScalableFloatSource>(GetSourceValue());
	return Source == EArcScalableFloatSource::InlineRange
		|| Source == EArcScalableFloatSource::DataTableRange
		|| Source == EArcScalableFloatSource::DataRegistryRange;
}

EVisibility FArcScalableFloatDetails::GetCustomScalingVisibility() const
{
	return !IsRangeSource() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FArcScalableFloatDetails::GetRangeWidgetVisibility() const
{
	return IsRangeSource() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FArcScalableFloatDetails::GetInlineRangeVisibility() const
{
	const EArcScalableFloatSource Source = static_cast<EArcScalableFloatSource>(GetSourceValue());
	return Source == EArcScalableFloatSource::InlineRange ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FArcScalableFloatDetails::GetDataTableRangePickerVisibility() const
{
	const EArcScalableFloatSource Source = static_cast<EArcScalableFloatSource>(GetSourceValue());
	return Source == EArcScalableFloatSource::DataTableRange ? EVisibility::Visible : EVisibility::Collapsed;
}

TSharedRef<SWidget> FArcScalableFloatDetails::GetDataTableRangePicker()
{
	const bool bAllowClear = true;
	TArray<const UClass*> AllowedClasses;
	AllowedClasses.Add(UDataTable::StaticClass());

	FAssetData CurrentAssetData;

	// Resolve current DataTable asset from the DataTable child handle (which wraps FDataTableRowHandle)
	TSharedPtr<IPropertyHandle> InnerDataTableHandle;
	if (DataTableProperty.IsValid())
	{
		InnerDataTableHandle = DataTableProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDataTableRowHandle, DataTable));
	}

	if (InnerDataTableHandle.IsValid())
	{
		UObject* DataTableObj = nullptr;
		InnerDataTableHandle->GetValue(DataTableObj);
		if (DataTableObj)
		{
			CurrentAssetData = FAssetData(DataTableObj);
		}
	}

	return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
		CurrentAssetData,
		bAllowClear,
		AllowedClasses,
		PropertyCustomizationHelpers::GetNewAssetFactoriesForClasses(AllowedClasses),
		FOnShouldFilterAsset::CreateSP(this, &FArcScalableFloatDetails::OnShouldFilterDataTableRangeAsset),
		FOnAssetSelected::CreateSP(this, &FArcScalableFloatDetails::OnSelectDataTableRange),
		FSimpleDelegate::CreateSP(this, &FArcScalableFloatDetails::OnCloseMenu));
}

void FArcScalableFloatDetails::OnSelectDataTableRange(const FAssetData& AssetData)
{
	if (!DataTableProperty.IsValid())
	{
		return;
	}

	TSharedPtr<IPropertyHandle> InnerDataTableHandle = DataTableProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDataTableRowHandle, DataTable));
	if (InnerDataTableHandle.IsValid())
	{
		UObject* SelectedAsset = AssetData.GetAsset();
		InnerDataTableHandle->SetValue(SelectedAsset);
	}

	// Clear the row name so a stale row from the previous table doesn't linger.
	if (DataTableRowNameProperty.IsValid())
	{
		DataTableRowNameProperty->SetValue(NAME_None);
	}

	FSlateApplication::Get().DismissAllMenus();
}

FText FArcScalableFloatDetails::GetDataTableRangeText() const
{
	if (!DataTableProperty.IsValid())
	{
		return LOCTEXT("PickDataTable", "Pick Range Table...");
	}

	TSharedPtr<IPropertyHandle> InnerDataTableHandle = DataTableProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDataTableRowHandle, DataTable));
	if (!InnerDataTableHandle.IsValid())
	{
		return LOCTEXT("PickDataTable", "Pick Range Table...");
	}

	UObject* DataTableObj = nullptr;
	InnerDataTableHandle->GetValue(DataTableObj);

	if (DataTableObj)
	{
		return FText::AsCultureInvariant(DataTableObj->GetName());
	}

	return LOCTEXT("PickDataTable", "Pick Range Table...");
}

FText FArcScalableFloatDetails::GetDataTableRangeTooltip() const
{
	return LOCTEXT("PickDataTableTooltip", "Select a DataTable whose row struct is FArcScalableFloatRange (Min/Max columns)");
}

bool FArcScalableFloatDetails::OnShouldFilterDataTableRangeAsset(const FAssetData& AssetData) const
{
	// Keep only DataTables whose row struct is FArcScalableFloatRange.
	// Filter returns true to EXCLUDE the asset.
	static const FName RowStructureTag = FName("RowStructure");
	FString RowStructValue;
	if (!AssetData.GetTagValue(RowStructureTag, RowStructValue))
	{
		// Older assets may use "RowStruct" tag name
		static const FName RowStructTag = FName("RowStruct");
		AssetData.GetTagValue(RowStructTag, RowStructValue);
	}

	if (RowStructValue.IsEmpty())
	{
		// No row struct tag — exclude
		return true;
	}

	const FString ExpectedPath = FArcScalableFloatRange::StaticStruct()->GetPathName();

	// The asset tag stores the short export path, e.g. "ArcScalableFloatRange" or the full class path.
	// Compare both the full path and the struct name to be resilient.
	return !RowStructValue.Contains(TEXT("ArcScalableFloatRange"));
}

EVisibility FArcScalableFloatDetails::GetDataRegistryRangeVisibility() const
{
	return GetSourceValue() == static_cast<uint8>(EArcScalableFloatSource::DataRegistryRange)
		? EVisibility::Visible : EVisibility::Collapsed;
}

FText FArcScalableFloatDetails::GetRangeRegistryRowText() const
{
	FName RowName = NAME_None;
	if (RangeRowNameProperty.IsValid())
	{
		RangeRowNameProperty->GetValue(RowName);
	}

	if (RowName.IsNone())
	{
		return LOCTEXT("PickRegistryRangeRow", "Pick registry row...");
	}

	return FText::FromName(RowName);
}

FDataRegistryId FArcScalableFloatDetails::GetRangeRegistryId(FPropertyAccess::Result* OutResult) const
{
	FPropertyAccess::Result TempResult;
	if (OutResult == nullptr)
	{
		OutResult = &TempResult;
	}

	FPropertyAccess::Result NameResult = FPropertyAccess::Fail;
	FName RowName = NAME_None;
	if (RangeRowNameProperty.IsValid())
	{
		NameResult = RangeRowNameProperty->GetValue(RowName);
	}

	FDataRegistryType RegistryType = GetRegistryType(OutResult);
	if (*OutResult == FPropertyAccess::Success)
	{
		*OutResult = NameResult;
	}

	return FDataRegistryId(RegistryType, RowName);
}

void FArcScalableFloatDetails::SetRangeRegistryId(FDataRegistryId NewId)
{
	if (RangeRowNameProperty.IsValid())
	{
		RangeRowNameProperty->SetValue(NewId.ItemName);
	}

	FDataRegistryType CurrentType = GetRegistryType();
	if (NewId.RegistryType != FDataRegistryType::CustomContextType && NewId.RegistryType != CurrentType)
	{
		RegistryTypeProperty->SetValueFromFormattedString(NewId.RegistryType.ToString());
	}
}

//-------------------------------------------------------------------------------------
// DataTableRange row-name combo helpers

void FArcScalableFloatDetails::GetDataTableRangeRowStrings(TArray<TSharedPtr<FString>>& OutStrings, TArray<TSharedPtr<class SToolTip>>& OutToolTips, TArray<bool>& OutRestrictedItems) const
{
	if (!DataTableProperty.IsValid())
	{
		return;
	}

	TSharedPtr<IPropertyHandle> InnerDataTableHandle = DataTableProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDataTableRowHandle, DataTable));
	if (!InnerDataTableHandle.IsValid())
	{
		return;
	}

	UObject* DataTableObj = nullptr;
	InnerDataTableHandle->GetValue(DataTableObj);

	UDataTable* DataTable = Cast<UDataTable>(DataTableObj);
	if (!DataTable)
	{
		return;
	}

	TArray<FName> RowNames;
	for (TMap<FName, uint8*>::TConstIterator It(DataTable->GetRowMap()); It; ++It)
	{
		RowNames.Add(It.Key());
	}

	RowNames.Sort(FNameLexicalLess());

	for (const FName& RowName : RowNames)
	{
		OutStrings.Add(MakeShared<FString>(RowName.ToString()));
		OutToolTips.Add(TSharedPtr<SToolTip>());
		OutRestrictedItems.Add(false);
	}
}

FString FArcScalableFloatDetails::GetDataTableRangeRowValueString() const
{
	if (!DataTableRowNameProperty.IsValid())
	{
		return LOCTEXT("SelectRow", "Select Row...").ToString();
	}

	FName RowName = NAME_None;
	DataTableRowNameProperty->GetValue(RowName);

	if (!RowName.IsNone())
	{
		return RowName.ToString();
	}

	return LOCTEXT("SelectRow", "Select Row...").ToString();
}

//-------------------------------------------------------------------------------------
// Inline curve widget and FCurveOwnerInterface

FRichCurve* FArcScalableFloatDetails::GetInlineRichCurve() const
{
	TArray<void*> RawData;
	if (InlineCurveProperty.IsValid())
	{
		InlineCurveProperty->AccessRawData(RawData);
	}
	return RawData.Num() == 1 ? static_cast<FRichCurve*>(RawData[0]) : nullptr;
}

EVisibility FArcScalableFloatDetails::GetInlineCurveVisibility() const
{
	return GetSourceValue() == static_cast<uint8>(EArcScalableFloatSource::InlineCurve)
		? EVisibility::Visible : EVisibility::Collapsed;
}

TSharedRef<SWidget> FArcScalableFloatDetails::CreateInlineCurveWidget()
{
	CurveOwnerObjects.Reset();
	TArray<UObject*> Outers;
	if (InlineCurveProperty.IsValid())
	{
		InlineCurveProperty->GetOuterObjects(Outers);
	}
	for (UObject* O : Outers)
	{
		CurveOwnerObjects.Add(O);
	}

	SAssignNew(CurveWidget, SCurveEditor)
		.ViewMinInput(0.f)
		.ViewMaxInput(MaxPreviewLevel)
		.ViewMinOutput(0.f)
		.ViewMaxOutput(1.f)
		.TimelineLength(MaxPreviewLevel)
		.DesiredSize(FVector2D(300.f, 150.f))
		.HideUI(false);

	if (CurveWidget.IsValid())
	{
		CurveWidget->SetCurveOwner(this, true);
		CurveWidget->SetPropertyUtils(PropertyUtilities.Pin());
		if (InlineCurveProperty.IsValid())
		{
			CurveWidget->RegisterToPropertyChangedEvent(InlineCurveProperty);
		}
	}

	return SNew(SBox)
		.Visibility(this, &FArcScalableFloatDetails::GetInlineCurveVisibility)
		[
			CurveWidget.IsValid() ? CurveWidget.ToSharedRef() : SNullWidget::NullWidget
		];
}

void FArcScalableFloatDetails::GetCurves(TAdderReserverRef<FRichCurveEditInfoConst> Curves) const
{
	FRichCurve* C = GetInlineRichCurve();
	if (C)
	{
		Curves.Add(FRichCurveEditInfoConst(C));
	}
}

TArray<FRichCurveEditInfo> FArcScalableFloatDetails::GetCurves()
{
	TArray<FRichCurveEditInfo> Out;
	FRichCurve* C = GetInlineRichCurve();
	if (C)
	{
		Out.Add(FRichCurveEditInfo(C));
	}
	return Out;
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
TArray<FRichCurveEditInfoConst> FArcScalableFloatDetails::GetCurves() const
{
	TArray<FRichCurveEditInfoConst> Out;
	FRichCurve* C = GetInlineRichCurve();
	if (C)
	{
		Out.Add(FRichCurveEditInfoConst(C));
	}
	return Out;
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void FArcScalableFloatDetails::ModifyOwner()
{
	for (TWeakObjectPtr<UObject> Weak : CurveOwnerObjects)
	{
		if (UObject* O = Weak.Get())
		{
			O->Modify();
		}
	}
}

TArray<const UObject*> FArcScalableFloatDetails::GetOwners() const
{
	TArray<const UObject*> Out;
	for (const TWeakObjectPtr<UObject>& Weak : CurveOwnerObjects)
	{
		if (const UObject* O = Weak.Get())
		{
			Out.Add(O);
		}
	}
	return Out;
}

void FArcScalableFloatDetails::MakeTransactional()
{
	for (TWeakObjectPtr<UObject> Weak : CurveOwnerObjects)
	{
		if (UObject* O = Weak.Get())
		{
			O->SetFlags(RF_Transactional);
		}
	}
}

void FArcScalableFloatDetails::OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos)
{
	if (InlineCurveProperty.IsValid())
	{
		InlineCurveProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
	}
}

bool FArcScalableFloatDetails::IsValidCurve(FRichCurveEditInfo CurveInfo)
{
	FRichCurve* C = GetInlineRichCurve();
	return C != nullptr && CurveInfo.CurveToEdit == static_cast<FRealCurve*>(C);
}

//-------------------------------------------------------------------------------------

#undef LOCTEXT_NAMESPACE
