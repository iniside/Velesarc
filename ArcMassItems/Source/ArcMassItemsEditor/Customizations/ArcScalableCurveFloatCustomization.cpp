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

#include "Customizations/ArcScalableCurveFloatCustomization.h"
#include "Customizations/SArcScalableCurveFloatPicker.h"
#include "Items/ArcItemScalableFloat.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/STextBlock.h"

namespace ArcScalableCurveFloatCustomizationInternal
{
	static FText BuildDisplayText(TSharedPtr<IPropertyHandle> InOwnerTypeHandle, TSharedPtr<IPropertyHandle> InScalableFloatHandle)
	{
		UObject* OwnerTypeObj = nullptr;
		if (InOwnerTypeHandle.IsValid())
		{
			InOwnerTypeHandle->GetValue(OwnerTypeObj);
		}

		UScriptStruct* OwnerStruct = Cast<UScriptStruct>(OwnerTypeObj);
		if (OwnerStruct == nullptr)
		{
			return FText::FromString(TEXT("None"));
		}

		FString FieldName;
		if (InScalableFloatHandle.IsValid())
		{
			TArray<void*> RawData;
			InScalableFloatHandle->AccessRawData(RawData);
			if (RawData.Num() > 0 && RawData[0] != nullptr)
			{
				const TFieldPath<FStructProperty>* FieldPath = reinterpret_cast<const TFieldPath<FStructProperty>*>(RawData[0]);
				if (FieldPath->Get() != nullptr)
				{
					FieldName = FieldPath->Get()->GetName();
				}
			}
		}

		if (FieldName.IsEmpty())
		{
			return FText::FromString(TEXT("None"));
		}

		return FText::FromString(FString::Printf(TEXT("%s.%s"), *OwnerStruct->GetName(), *FieldName));
	}
}

TSharedRef<IPropertyTypeCustomization> FArcScalableCurveFloatCustomization::MakeInstance()
{
	return MakeShareable(new FArcScalableCurveFloatCustomization());
}

void FArcScalableCurveFloatCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	OwnerTypeHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcScalableCurveFloat, OwnerType));
	ScalableFloatHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcScalableCurveFloat, ScalableFloat));

	HeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(200.f)
	[
		SAssignNew(ComboButton, SComboButton)
		.OnGetMenuContent_Lambda([this]()
		{
			return SNew(SBox)
			.MinDesiredWidth(300.f)
			.MaxDesiredHeight(400.f)
			[
				SNew(SArcScalableCurveFloatPicker)
				.OnSelected(this, &FArcScalableCurveFloatCustomization::OnScalableFloatSelected)
			];
		})
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(this, &FArcScalableCurveFloatCustomization::GetDisplayText)
			.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
		]
	];
}

void FArcScalableCurveFloatCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

void FArcScalableCurveFloatCustomization::OnScalableFloatSelected(UScriptStruct* OwnerType, FStructProperty* Prop)
{
	if (OwnerTypeHandle.IsValid())
	{
		OwnerTypeHandle->SetValue(static_cast<UObject*>(OwnerType));
	}

	if (ScalableFloatHandle.IsValid() && Prop != nullptr)
	{
		ScalableFloatHandle->NotifyPreChange();

		TArray<void*> RawData;
		ScalableFloatHandle->AccessRawData(RawData);
		for (void* Ptr : RawData)
		{
			if (Ptr != nullptr)
			{
				TFieldPath<FStructProperty>* FieldPath = reinterpret_cast<TFieldPath<FStructProperty>*>(Ptr);
				*FieldPath = TFieldPath<FStructProperty>(Prop);
			}
		}

		ScalableFloatHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
		ScalableFloatHandle->NotifyFinishedChangingProperties();
	}

	if (ComboButton.IsValid())
	{
		ComboButton->SetIsOpen(false);
	}
}

FText FArcScalableCurveFloatCustomization::GetDisplayText() const
{
	return ArcScalableCurveFloatCustomizationInternal::BuildDisplayText(OwnerTypeHandle, ScalableFloatHandle);
}
