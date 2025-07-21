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
#include "DataRegistryId.h"

class FArcScalableFloatDetails : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
	static constexpr float DefaultMinPreviewLevel = 0.f;
	static constexpr float DefaultMaxPreviewLevel = 30.f;

	FArcScalableFloatDetails()
		: PreviewLevel(0.f)
		, MinPreviewLevel(DefaultMinPreviewLevel)
		, MaxPreviewLevel(DefaultMaxPreviewLevel) 
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

	TSharedPtr<IPropertyHandle> ValueProperty;
	TSharedPtr<IPropertyHandle> CurveTableHandleProperty;
	TSharedPtr<IPropertyHandle> CurveTableProperty;
	TSharedPtr<IPropertyHandle> RowNameProperty;
	TSharedPtr<IPropertyHandle> RegistryTypeProperty;

	TWeakPtr<IPropertyUtilities> PropertyUtilities;

	float PreviewLevel;
	float MinPreviewLevel;
	float MaxPreviewLevel;
	bool bSourceRefreshQueued;
};
