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

#pragma once
#include "Curves/CurveOwnerInterface.h"
#include "DataRegistryId.h"
#include "IPropertyTypeCustomization.h"
#include "PropertyEditorModule.h"

class FArcScalableFloatDetails : public IPropertyTypeCustomization, public FCurveOwnerInterface
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
	static constexpr float DefaultMinPreviewLevel = 0.f;
	static constexpr float DefaultMaxPreviewLevel = 30.f;

	FArcScalableFloatDetails()
		: PreviewLevel(0.f)
		, MinPreviewLevel(DefaultMinPreviewLevel)
		, MaxPreviewLevel(DefaultMaxPreviewLevel)
		, bSourceRefreshQueued(false)
		, bSourceModeResetQueued(false)
	{
	}

protected:

	virtual void CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;
	virtual void CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;

	bool IsEditable() const;
	void UpdatePreviewLevels();

	// Curve Table selector
	TSharedRef<SWidget> CreateCurveTableWidget();
	TSharedRef<SWidget> GetCurveTablePicker();
	void OnSelectCurveTable(const FAssetData& AssetData);
	void OnCloseMenu();
	FText GetCurveTableText() const;
	FText GetCurveTableTooltip() const;
	EVisibility GetCurveTableVisiblity() const;
	EVisibility GetAssetButtonVisiblity() const;
	void OnBrowseTo();
	void OnClear();
	void OnUseSelected();

	// Registry Type selector
	TSharedRef<SWidget> CreateRegistryTypeWidget();
	FString GetRegistryTypeValueString() const;
	FText GetRegistryTypeTooltip() const;
	EVisibility GetRegistryTypeVisiblity() const;

	// Curve source accessors
	void OnCurveSourceChanged();
	void RefreshSourceData();
	class UCurveTable* GetCurveTable(FPropertyAccess::Result* OutResult = nullptr) const;
	FDataRegistryType GetRegistryType(FPropertyAccess::Result* OutResult = nullptr) const;

	// Row/item name widget
	TSharedRef<SWidget> CreateRowNameWidget();
	EVisibility GetRowNameVisibility() const;
	FText GetRowNameComboBoxContentText() const;
	FText GetRowNameComboBoxContentTooltip() const;
	void OnRowNameChanged();

	// Range source widgets (InlineRange / DataTableRange / DataRegistryRange)
	TSharedRef<SWidget> CreateRangeWidget();
	EVisibility GetRangeWidgetVisibility() const;
	EVisibility GetDataTableRangePickerVisibility() const;
	EVisibility GetInlineRangeVisibility() const;
	bool IsRangeSource() const;

	// DataTableRange picker callbacks
	TSharedRef<SWidget> GetDataTableRangePicker();
	void OnSelectDataTableRange(const FAssetData& AssetData);
	FText GetDataTableRangeText() const;
	FText GetDataTableRangeTooltip() const;
	bool OnShouldFilterDataTableRangeAsset(const FAssetData& AssetData) const;

	// DataTableRange row picker (PropertyComboBox backed by actual row names from selected DataTable)
	TSharedRef<SWidget> CreateDataTableRangeRowWidget();
	void GetDataTableRangeRowStrings(TArray<TSharedPtr<FString>>& OutStrings, TArray<TSharedPtr<class SToolTip>>& OutToolTips, TArray<bool>& OutRestrictedItems) const;
	FString GetDataTableRangeRowValueString() const;

	// DataRegistryRange picker callbacks
	TSharedRef<SWidget> CreateRangeRegistryTypeWidget();
	TSharedRef<SWidget> CreateRangeRegistryRowWidget();
	EVisibility GetDataRegistryRangeVisibility() const;
	FText GetRangeRegistryRowText() const;
	FDataRegistryId GetRangeRegistryId(FPropertyAccess::Result* OutResult = nullptr) const;
	void SetRangeRegistryId(FDataRegistryId NewId);

	// CustomScaling slot visibility
	EVisibility GetCustomScalingVisibility() const;

	// Inline curve widget
	TSharedRef<SWidget> CreateInlineCurveWidget();
	EVisibility GetInlineCurveVisibility() const;
	FRichCurve* GetInlineRichCurve() const;

	// FCurveOwnerInterface overrides
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	virtual TArray<FRichCurveEditInfoConst> GetCurves() const override;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	virtual void GetCurves(TAdderReserverRef<FRichCurveEditInfoConst> Curves) const override;
	virtual TArray<FRichCurveEditInfo> GetCurves() override;
	virtual void ModifyOwner() override;
	virtual TArray<const UObject*> GetOwners() const override;
	virtual void MakeTransactional() override;
	virtual void OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos) override;
	virtual bool IsValidCurve(FRichCurveEditInfo CurveInfo) override;

	// Preview widgets
	EVisibility GetPreviewVisibility() const;
	float GetPreviewLevel() const;
	void SetPreviewLevel(float NewLevel);
	FText GetRowValuePreviewLabel() const;
	FText GetRowValuePreviewText() const;

	// Row accessors and callbacks
	FName GetRowName(FPropertyAccess::Result* OutResult = nullptr) const;
	const FRealCurve* GetRealCurve(FPropertyAccess::Result* OutResult = nullptr) const;
	FDataRegistryId GetRegistryId(FPropertyAccess::Result* OutResult = nullptr) const;
	void SetRegistryId(FDataRegistryId NewId);
	void GetCustomRowNames(TArray<FName>& OutRows) const;

	// Reads the current EArcScalableFloatSource value from SourceProperty (uint8 cast).
	uint8 GetSourceValue() const;

	// Source-mode reset — fired when the Source combo changes.
	void OnSourceModeChanged();
	void ApplySourceModeReset();

	TSharedPtr<IPropertyHandle> ValueProperty;
	TSharedPtr<IPropertyHandle> CurveTableHandleProperty;
	TSharedPtr<IPropertyHandle> CurveTableProperty;
	TSharedPtr<IPropertyHandle> RowNameProperty;
	TSharedPtr<IPropertyHandle> RegistryTypeProperty;
	TSharedPtr<IPropertyHandle> SourceProperty;
	TSharedPtr<IPropertyHandle> CustomScalingProperty;
	TSharedPtr<IPropertyTypeCustomization> InstancedStructCustomization;

	// New range-source child handles
	TSharedPtr<IPropertyHandle> MinValueProperty;
	TSharedPtr<IPropertyHandle> MaxValueProperty;
	TSharedPtr<IPropertyHandle> DataTableProperty;
	TSharedPtr<IPropertyHandle> DataTableRowNameProperty;
	TSharedPtr<IPropertyHandle> RangeRowNameProperty;

	// Inline curve editing
	TSharedPtr<IPropertyHandle> InlineCurveProperty;
	TSharedPtr<class SCurveEditor> CurveWidget;
	TArray<TWeakObjectPtr<UObject>> CurveOwnerObjects;

	TWeakPtr<IPropertyUtilities> PropertyUtilities;

	float PreviewLevel;
	float MinPreviewLevel;
	float MaxPreviewLevel;
	bool bSourceRefreshQueued;
	bool bSourceModeResetQueued;
};
