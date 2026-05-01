// Copyright Lukasz Baran. All Rights Reserved.

#include "Customizations/ArcAttributeRefCustomization.h"
#include "Customizations/SArcAttributeRefPicker.h"
#include "Attributes/ArcAttribute.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<IPropertyTypeCustomization> FArcAttributeRefCustomization::MakeInstance()
{
	return MakeShareable(new FArcAttributeRefCustomization());
}

void FArcAttributeRefCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	FragmentTypeHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcAttributeRef, FragmentType));
	PropertyNameHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcAttributeRef, PropertyName));

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
				SNew(SArcAttributeRefPicker)
				.OnAttributeSelected(this, &FArcAttributeRefCustomization::OnAttributeSelected)
			];
		})
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(this, &FArcAttributeRefCustomization::GetDisplayText)
			.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
		]
	];
}

void FArcAttributeRefCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

void FArcAttributeRefCustomization::OnAttributeSelected(UScriptStruct* FragmentType, FName PropertyName)
{
	if (FragmentTypeHandle.IsValid())
	{
		FragmentTypeHandle->SetValue(FragmentType);
	}
	if (PropertyNameHandle.IsValid())
	{
		PropertyNameHandle->SetValue(PropertyName);
	}

	if (ComboButton.IsValid())
	{
		ComboButton->SetIsOpen(false);
	}
}

FText FArcAttributeRefCustomization::GetDisplayText() const
{
	UObject* FragmentTypeObj = nullptr;
	if (FragmentTypeHandle.IsValid())
	{
		FragmentTypeHandle->GetValue(FragmentTypeObj);
	}

	FName PropName = NAME_None;
	if (PropertyNameHandle.IsValid())
	{
		PropertyNameHandle->GetValue(PropName);
	}

	UScriptStruct* FragStruct = Cast<UScriptStruct>(FragmentTypeObj);
	if (FragStruct && PropName != NAME_None)
	{
		return FText::FromString(FString::Printf(TEXT("%s.%s"), *FragStruct->GetName(), *PropName.ToString()));
	}

	return FText::FromString(TEXT("None"));
}
