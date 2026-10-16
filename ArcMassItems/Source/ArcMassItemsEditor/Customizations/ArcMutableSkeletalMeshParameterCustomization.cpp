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

#include "Customizations/ArcMutableSkeletalMeshParameterCustomization.h"
#include "Customizations/ArcItemRecordCustomizableObject.h"
#include "Equipment/Fragments/ArcItemFragment_CustomizableObject.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectParameterTypeDefinitions.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyTypeCustomization.h"
#include "Widgets/Input/SSearchableComboBox.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<IPropertyTypeCustomization> FArcMutableSkeletalMeshParameterCustomization::MakeInstance()
{
	return MakeShareable(new FArcMutableSkeletalMeshParameterCustomization());
}

UCustomizableObject* FArcMutableSkeletalMeshParameterCustomization::GetCustomizableObject() const
{
	return ArcItemRecordCustomizableObject::Find(MyHandle);
}

void FArcMutableSkeletalMeshParameterCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	MyHandle = StructPropertyHandle;
	ParameterNameHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcMutableSkeletalMeshParameter, ParameterName));
	SkeletalMeshHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcMutableSkeletalMeshParameter, SkeletalMesh));
	RangeIndexHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcMutableSkeletalMeshParameter, RangeIndex));

	HeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(300.f)
	[
		StructPropertyHandle->CreatePropertyValueWidget()
	];
}

void FArcMutableSkeletalMeshParameterCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructBuilder.AddCustomRow(NSLOCTEXT("ArcMutableSkeletalMeshParameter", "Param", "Parameter"))
	.NameContent()
	[
		ParameterNameHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(250.f)
	[
		SAssignNew(ParameterCombo, SSearchableComboBox)
		.OptionsSource(&ParameterOptions)
		.OnComboBoxOpening(this, &FArcMutableSkeletalMeshParameterCustomization::OnParameterComboOpening)
		.OnSelectionChanged(this, &FArcMutableSkeletalMeshParameterCustomization::OnParameterChanged)
		.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
		{
			return SNew(STextBlock)
				.Text(FText::FromString(Item.IsValid() ? *Item : FString()));
		})
		.Content()
		[
			SNew(STextBlock)
			.Text(this, &FArcMutableSkeletalMeshParameterCustomization::GetParameterText)
		]
	];

	StructBuilder.AddProperty(SkeletalMeshHandle.ToSharedRef());
	StructBuilder.AddProperty(RangeIndexHandle.ToSharedRef());

	RefreshParameterOptions();
}

void FArcMutableSkeletalMeshParameterCustomization::RefreshParameterOptions()
{
	ParameterOptions.Reset();

	UCustomizableObject* CO = GetCustomizableObject();
	if (CO != nullptr)
	{
		const int32 Count = CO->GetParameterCount();
		for (int32 i = 0; i < Count; ++i)
		{
			const FString& Name = CO->GetParameterName(i);
			if (CO->GetParameterTypeByName(Name) == EMutableParameterType::SkeletalMesh)
			{
				ParameterOptions.Add(MakeShared<FString>(Name));
			}
		}
	}

	if (ParameterCombo.IsValid())
	{
		ParameterCombo->RefreshOptions();
	}
}

void FArcMutableSkeletalMeshParameterCustomization::OnParameterComboOpening()
{
	RefreshParameterOptions();
}

void FArcMutableSkeletalMeshParameterCustomization::OnParameterChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	if (NewValue.IsValid())
	{
		ParameterNameHandle->SetValue(*NewValue);
	}
}

FText FArcMutableSkeletalMeshParameterCustomization::GetParameterText() const
{
	if (!ParameterNameHandle.IsValid())
	{
		return NSLOCTEXT("ArcMutableSkeletalMeshParameter", "NoneParam", "None");
	}

	FString Value;
	ParameterNameHandle->GetValue(Value);
	if (Value.IsEmpty())
	{
		return NSLOCTEXT("ArcMutableSkeletalMeshParameter", "NoneParam", "None");
	}

	return FText::FromString(Value);
}
