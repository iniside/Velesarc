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

#include "ArcSvgAssetDetailCustomization.h"

#include "Brushes/SlateImageBrush.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "PropertyHandle.h"
#include "Svg/ArcSvgAsset.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ArcSvgAssetDetail"

TSharedRef<IDetailCustomization> FArcSvgAssetDetailCustomization::MakeInstance()
{
	return MakeShared<FArcSvgAssetDetailCustomization>();
}

void FArcSvgAssetDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() != 1 || !Objects[0].IsValid())
	{
		return;
	}

	UArcSvgAsset* Asset = Cast<UArcSvgAsset>(Objects[0].Get());
	if (Asset == nullptr)
	{
		return;
	}
	WeakAsset = Asset;

	const FString& Path = Asset->GetOrExtractSvgFilePath();
	if (!Path.IsEmpty())
	{
		PreviewBrush = MakeShared<FSlateVectorImageBrush>(Path, FVector2D(128.f, 128.f), FLinearColor::White);
	}

	IDetailCategoryBuilder& PreviewCat = DetailBuilder.EditCategory(
		"Preview", LOCTEXT("PreviewCat", "Preview"), ECategoryPriority::Important);

	if (PreviewBrush.IsValid())
	{
		PreviewCat.AddCustomRow(LOCTEXT("SvgPreviewRow", "SVG Preview"))
			.WholeRowContent()
			.HAlign(HAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(128.f)
				.HeightOverride(128.f)
				[
					SAssignNew(PreviewImage, SImage)
					.Image(PreviewBrush.Get())
					.ColorAndOpacity(Asset->Tint)
				]
			];
	}
	else
	{
		PreviewCat.AddCustomRow(LOCTEXT("SvgPreviewRow", "SVG Preview"))
			.WholeRowContent()
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoSvg", "No SVG data"))
			];
	}

	// Live tint: lead with the whole-property change (FLinearColor is rendered by
	// FColorStructCustomization, which fires on commit), plus per-component child changes.
	TSharedRef<IPropertyHandle> TintHandle = DetailBuilder.GetProperty(
		GET_MEMBER_NAME_CHECKED(UArcSvgAsset, Tint));
	TintHandle->SetOnPropertyValueChanged(
		FSimpleDelegate::CreateSP(this, &FArcSvgAssetDetailCustomization::OnTintChanged));
	TintHandle->SetOnChildPropertyValueChanged(
		FSimpleDelegate::CreateSP(this, &FArcSvgAssetDetailCustomization::OnTintChanged));
}

void FArcSvgAssetDetailCustomization::OnTintChanged()
{
	if (!WeakAsset.IsValid() || !PreviewImage.IsValid())
	{
		return;
	}
	PreviewImage->SetColorAndOpacity(WeakAsset->Tint);
}

#undef LOCTEXT_NAMESPACE
