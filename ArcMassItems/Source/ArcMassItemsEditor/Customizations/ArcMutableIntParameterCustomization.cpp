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

#include "Customizations/ArcMutableIntParameterCustomization.h"
#include "Customizations/ArcItemRecordCustomizableObject.h"
#include "Equipment/Fragments/ArcItemFragment_CustomizableObject.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectParameterTypeDefinitions.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyTypeCustomization.h"
#include "Widgets/Input/SSearchableComboBox.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<IPropertyTypeCustomization> FArcMutableIntParameterCustomization::MakeInstance()
{
	return MakeShareable(new FArcMutableIntParameterCustomization());
}

UCustomizableObject* FArcMutableIntParameterCustomization::GetCustomizableObject() const
{
	return ArcItemRecordCustomizableObject::Find(MyHandle);
}

void FArcMutableIntParameterCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	MyHandle = StructPropertyHandle;
	ParameterNameHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcMutableIntParameter, ParameterName));
	SelectedOptionHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcMutableIntParameter, SelectedOption));

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

void FArcMutableIntParameterCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructBuilder.AddCustomRow(NSLOCTEXT("ArcMutableIntParameter", "Param", "Parameter"))
	.NameContent()
	[
		ParameterNameHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(250.f)
	[
		SAssignNew(ParameterCombo, SSearchableComboBox)
		.OptionsSource(&ParameterOptions)
		.OnComboBoxOpening(this, &FArcMutableIntParameterCustomization::OnParameterComboOpening)
		.OnSelectionChanged(this, &FArcMutableIntParameterCustomization::OnParameterChanged)
		.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
		{
			return SNew(STextBlock)
				.Text(FText::FromString(Item.IsValid() ? *Item : FString()));
		})
		.Content()
		[
			SNew(STextBlock)
			.Text(this, &FArcMutableIntParameterCustomization::GetParameterText)
		]
	];

	StructBuilder.AddCustomRow(NSLOCTEXT("ArcMutableIntParameter", "Value", "Selected Option"))
	.NameContent()
	[
		SelectedOptionHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(250.f)
	[
		SAssignNew(ValueCombo, SSearchableComboBox)
		.OptionsSource(&ValueOptions)
		.OnComboBoxOpening(this, &FArcMutableIntParameterCustomization::OnValueComboOpening)
		.OnSelectionChanged(this, &FArcMutableIntParameterCustomization::OnValueChanged)
		.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
		{
			return SNew(STextBlock)
				.Text(FText::FromString(Item.IsValid() ? *Item : FString()));
		})
		.Content()
		[
			SNew(STextBlock)
			.Text(this, &FArcMutableIntParameterCustomization::GetValueText)
		]
	];

	RefreshParameterOptions();
	RefreshValueOptions();
}

void FArcMutableIntParameterCustomization::RefreshParameterOptions()
{
	ParameterOptions.Reset();

	UCustomizableObject* CO = GetCustomizableObject();
	if (CO != nullptr)
	{
		const int32 Count = CO->GetParameterCount();
		for (int32 i = 0; i < Count; ++i)
		{
			const FString& Name = CO->GetParameterName(i);
			if (CO->GetParameterTypeByName(Name) == EMutableParameterType::Int)
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

void FArcMutableIntParameterCustomization::RefreshValueOptions()
{
	ValueOptions.Reset();
	ValueOptions.Add(MakeShared<FString>(TEXT("None")));

	UCustomizableObject* CO = GetCustomizableObject();
	if (CO != nullptr && ParameterNameHandle.IsValid())
	{
		FString CurrentParam;
		ParameterNameHandle->GetValue(CurrentParam);
		if (!CurrentParam.IsEmpty())
		{
			const int32 NumValues = CO->GetEnumParameterNumValues(CurrentParam);
			for (int32 j = 0; j < NumValues; ++j)
			{
				ValueOptions.Add(MakeShared<FString>(CO->GetEnumParameterValue(CurrentParam, j)));
			}
		}
	}

	if (ValueCombo.IsValid())
	{
		ValueCombo->RefreshOptions();
	}
}

void FArcMutableIntParameterCustomization::OnParameterComboOpening()
{
	RefreshParameterOptions();
}

void FArcMutableIntParameterCustomization::OnValueComboOpening()
{
	RefreshValueOptions();
}

void FArcMutableIntParameterCustomization::OnParameterChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	if (NewValue.IsValid())
	{
		ParameterNameHandle->SetValue(*NewValue);
	}
	SelectedOptionHandle->SetValue(FString());
	RefreshValueOptions();
}

void FArcMutableIntParameterCustomization::OnValueChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	if (NewValue.IsValid())
	{
		SelectedOptionHandle->SetValue(*NewValue);
	}
}

FText FArcMutableIntParameterCustomization::GetParameterText() const
{
	if (!ParameterNameHandle.IsValid())
	{
		return NSLOCTEXT("ArcMutableIntParameter", "NoneParam", "None");
	}

	FString Value;
	ParameterNameHandle->GetValue(Value);
	if (Value.IsEmpty())
	{
		return NSLOCTEXT("ArcMutableIntParameter", "NoneParam", "None");
	}

	return FText::FromString(Value);
}

FText FArcMutableIntParameterCustomization::GetValueText() const
{
	if (!SelectedOptionHandle.IsValid())
	{
		return NSLOCTEXT("ArcMutableIntParameter", "NoneValue", "None");
	}

	FString Value;
	SelectedOptionHandle->GetValue(Value);
	if (Value.IsEmpty())
	{
		return NSLOCTEXT("ArcMutableIntParameter", "NoneValue", "None");
	}

	return FText::FromString(Value);
}
