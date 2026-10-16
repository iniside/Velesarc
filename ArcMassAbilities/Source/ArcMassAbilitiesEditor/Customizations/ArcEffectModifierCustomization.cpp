// Copyright Lukasz Baran. All Rights Reserved.

#include "Customizations/ArcEffectModifierCustomization.h"
#include "Customizations/ArcEffectSummaryText.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "PropertyHandle.h"
#include "Widgets/Text/STextBlock.h"
#include "Effects/ArcEffectModifier.h"
#include "Effects/ArcEffectTypes.h"
#include "ArcScalableFloat.h"
#include "GameplayTagContainer.h"

TSharedRef<IPropertyTypeCustomization> FArcEffectModifierCustomization::MakeInstance()
{
	return MakeShareable(new FArcEffectModifierCustomization());
}

void FArcEffectModifierCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	AttributeHandle      = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcEffectModifier, Attribute));
	OperationHandle      = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcEffectModifier, Operation));
	TypeHandle           = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcEffectModifier, Type));
	MagnitudeHandle      = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcEffectModifier, Magnitude));
	SetByCallerTagHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcEffectModifier, SetByCallerTag));
	CustomMagnitudeHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcEffectModifier, CustomMagnitude));

	HeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(260.f)
	[
		SNew(STextBlock)
		.Text(this, &FArcEffectModifierCustomization::GetSummaryText)
		.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
	];
}

void FArcEffectModifierCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 NumChildren = 0;
	StructPropertyHandle->GetNumChildren(NumChildren);

	for (uint32 Index = 0; Index < NumChildren; ++Index)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(Index);
		if (!ChildHandle.IsValid())
		{
			continue;
		}

		const FName ChildName = ChildHandle->GetProperty() ? ChildHandle->GetProperty()->GetFName() : NAME_None;
		if (ChildName == GET_MEMBER_NAME_CHECKED(FArcEffectModifier, SourceTagRequirements) ||
			ChildName == GET_MEMBER_NAME_CHECKED(FArcEffectModifier, TargetTagRequirements))
		{
			IDetailPropertyRow& Row = StructBuilder.AddProperty(ChildHandle.ToSharedRef());
			TSharedPtr<IPropertyHandle> TagHandle = ChildHandle;
			Row.CustomWidget(/*bShowChildren=*/true)
			   .NameContent()
			   [
				   ChildHandle->CreatePropertyNameWidget()
			   ]
			   .ValueContent()
			   [
				   SNew(STextBlock)
				   .Text_Lambda([TagHandle]()
				   {
					   return ArcEffectSummaryText::FormatTagRequirements(TagHandle);
				   })
				   .ToolTipText_Lambda([TagHandle]()
				   {
					   return ArcEffectSummaryText::BuildTagRequirementsTooltip(TagHandle);
				   })
				   .Font(IPropertyTypeCustomizationUtils::GetRegularFont())
			   ];
		}
		else
		{
			StructBuilder.AddProperty(ChildHandle.ToSharedRef());
		}
	}
}

FText FArcEffectModifierCustomization::GetSummaryText() const
{
	FText AttrText = ArcEffectSummaryText::FormatAttributeRef(AttributeHandle);
	FText OpText   = ArcEffectSummaryText::OperationToText(OperationHandle);
	FText MagText  = ArcEffectSummaryText::BuildModifierMagnitude(
		TypeHandle, MagnitudeHandle, SetByCallerTagHandle, CustomMagnitudeHandle);

	FString Result;
	Result.Reserve(128);

	if (!AttrText.IsEmpty())
	{
		Result += AttrText.ToString();
	}

	if (!OpText.IsEmpty())
	{
		if (!Result.IsEmpty())
		{
			Result += TEXT("  ");
		}
		Result += OpText.ToString();
	}

	if (!MagText.IsEmpty())
	{
		if (!Result.IsEmpty())
		{
			Result += TEXT("  ");
		}
		Result += MagText.ToString();
	}

	return FText::FromString(Result);
}
