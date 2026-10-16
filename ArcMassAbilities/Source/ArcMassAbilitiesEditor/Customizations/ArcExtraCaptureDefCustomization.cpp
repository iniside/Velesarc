// Copyright Lukasz Baran. All Rights Reserved.

#include "Customizations/ArcExtraCaptureDefCustomization.h"
#include "Customizations/ArcEffectSummaryText.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "Widgets/Text/STextBlock.h"
#include "Effects/ArcExtraCapture.h"

TSharedRef<IPropertyTypeCustomization> FArcExtraCaptureDefCustomization::MakeInstance()
{
	return MakeShareable(new FArcExtraCaptureDefCustomization());
}

void FArcExtraCaptureDefCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	SourceHandle   = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcExtraCaptureDef, Source));
	SnapshotHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcExtraCaptureDef, bSnapshot));

	HeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(260.f)
	[
		SNew(STextBlock)
		.Text(this, &FArcExtraCaptureDefCustomization::GetSummaryText)
		.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
	];
}

void FArcExtraCaptureDefCustomization::CustomizeChildren(
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

FText FArcExtraCaptureDefCustomization::GetSummaryText() const
{
	FText SourceText = ArcEffectSummaryText::InstancedStructTypeName(SourceHandle);

	bool bSnap = false;
	if (SnapshotHandle.IsValid())
	{
		SnapshotHandle->GetValue(bSnap);
	}

	FString Result;
	Result.Reserve(128);

	if (bSnap)
	{
		Result += TEXT("[snapshot]  ·  ");
	}

	Result += SourceText.ToString();

	return FText::FromString(Result);
}
