// Copyright Lukasz Baran. All Rights Reserved.

#include "Customizations/ArcEffectExecutionEntryCustomization.h"
#include "Customizations/ArcEffectSummaryText.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "Widgets/Text/STextBlock.h"
#include "Effects/ArcEffectDefinition.h"

TSharedRef<IPropertyTypeCustomization> FArcEffectExecutionEntryCustomization::MakeInstance()
{
	return MakeShareable(new FArcEffectExecutionEntryCustomization());
}

void FArcEffectExecutionEntryCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	ModifiersHandle       = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcEffectExecutionEntry, Modifiers));
	CustomExecutionHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcEffectExecutionEntry, CustomExecution));

	HeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(260.f)
	[
		SNew(STextBlock)
		.Text(this, &FArcEffectExecutionEntryCustomization::GetSummaryText)
		.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
	];
}

void FArcEffectExecutionEntryCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 NumChildren = 0;
	StructPropertyHandle->GetNumChildren(NumChildren);

	for (uint32 Index = 0; Index < NumChildren; ++Index)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(Index);
		if (ChildHandle.IsValid())
		{
			StructBuilder.AddProperty(ChildHandle.ToSharedRef());
		}
	}
}

FText FArcEffectExecutionEntryCustomization::GetSummaryText() const
{
	uint32 Num = 0;
	if (ModifiersHandle.IsValid())
	{
		ModifiersHandle->GetNumChildren(Num);
	}

	FText TypeText = ArcEffectSummaryText::InstancedStructTypeName(CustomExecutionHandle);

	return FText::FromString(FString::Printf(TEXT("%d mods  ·  %s"), Num, *TypeText.ToString()));
}
