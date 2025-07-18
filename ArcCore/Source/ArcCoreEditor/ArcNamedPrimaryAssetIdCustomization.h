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
#include "IPropertyTypeCustomization.h"
#include "SGraphPin.h"
#include "UObject/PrimaryAssetId.h"
#include "ArcNamedPrimaryAssetId.h"

class FAssetThumbnail;
class FDetailWidgetRow;
class IDetailChildrenBuilder;
class IPropertyHandle;
class SBorder;

class FArcNamedPrimaryAssetIdCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FArcNamedPrimaryAssetIdCustomization);
	}

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override {}

private:
	void OnIdSelected(FPrimaryAssetId AssetId);
	void OnBrowseTo();
	void OnClear();
	void OnUseSelected();
	FText GetDisplayText() const;
	FText GetDisplayTooltipText() const;
	FPrimaryAssetId GetCurrentPrimaryAssetId() const;
	void UpdateThumbnail();
	void OnOpenAssetEditor();

	/** Gets the border brush to show around the thumbnail, changes when the user hovers on it. */
	const FSlateBrush* GetThumbnailBorder() const;

	/**
	 * Handle double clicking the asset thumbnail. this 'Edits' the displayed asset
	 */
	FReply OnAssetThumbnailDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent);

	/** Handle to the struct property being customized */
	TSharedPtr<IPropertyHandle> AssetIdPropertyHandle;

	TSharedPtr<IPropertyHandle> StructPropertyHandle;
	
	TSharedPtr<IPropertyHandle> NamePropertyHandle;
	
	/** Specified type */
	TArray<FPrimaryAssetType> AllowedTypes;

	/** Classes which can be selected with this PrimaryAssetId  */
	TArray<const UClass*> AllowedClasses;

	/** Classes which cannot be selected with this PrimaryAssetId  */
	TArray<const UClass*> DisallowedClasses;
	
	/** Thumbnail resource */
	TSharedPtr<FAssetThumbnail> AssetThumbnail;

	/** The border surrounding the thumbnail image. */
	TSharedPtr<SBorder> ThumbnailBorder;
	
};
