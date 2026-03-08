// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcScalableCurveFloatDetails.h"
#include "SArcScalableCurveFloatWidget.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"

#include "Items/ArcItemScalableFloat.h"

#define LOCTEXT_NAMESPACE "ArcScalableCurveFloatDetails"

TSharedRef<IPropertyTypeCustomization> FArcScalableCurveFloatDetails::MakeInstance()
{
	return MakeShareable(new FArcScalableCurveFloatDetails());
}

void FArcScalableCurveFloatDetails::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	ScalableFloatProperty = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcScalableCurveFloat, ScalableFloat));
	CurvePropProperty = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcScalableCurveFloat, CurveProp));
	OwnerTypeProperty = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcScalableCurveFloat, OwnerType));
	TypeProperty = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcScalableCurveFloat, Type));

	// Read current value for default selection
	FProperty* CurrentProperty = nullptr;
	UScriptStruct* CurrentOwnerStruct = nullptr;

	if (OwnerTypeProperty.IsValid())
	{
		UObject* ObjValue = nullptr;
		OwnerTypeProperty->GetValue(ObjValue);
		CurrentOwnerStruct = Cast<UScriptStruct>(ObjValue);
	}

	if (ScalableFloatProperty.IsValid())
	{
		FProperty* PropValue = nullptr;
		ScalableFloatProperty->GetValue(PropValue);
		if (PropValue)
		{
			CurrentProperty = PropValue;
		}
	}

	if (!CurrentProperty && CurvePropProperty.IsValid())
	{
		FProperty* PropValue = nullptr;
		CurvePropProperty->GetValue(PropValue);
		if (PropValue)
		{
			CurrentProperty = PropValue;
		}
	}

	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(250)
		.MaxDesiredWidth(4096)
		[
			SNew(SArcScalableCurveFloatWidget)
			.OnSelectionChanged(this, &FArcScalableCurveFloatDetails::OnSelectionChanged)
			.DefaultProperty(CurrentProperty)
			.DefaultOwnerStruct(CurrentOwnerStruct)
		];
}

void FArcScalableCurveFloatDetails::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

void FArcScalableCurveFloatDetails::OnSelectionChanged(FProperty* SelectedProperty, UScriptStruct* OwnerStruct, EArcScalableType Type)
{
	if (TypeProperty.IsValid())
	{
		TypeProperty->SetValue(static_cast<uint8>(Type));
	}

	if (OwnerTypeProperty.IsValid())
	{
		UObject* StructObj = OwnerStruct;
		OwnerTypeProperty->SetValue(StructObj);
	}

	if (ScalableFloatProperty.IsValid())
	{
		if (Type == EArcScalableType::Scalable)
		{
			ScalableFloatProperty->SetValue(SelectedProperty);
		}
		else
		{
			UObject* NullObj = nullptr;
			ScalableFloatProperty->SetValue(NullObj);
		}
	}

	if (CurvePropProperty.IsValid())
	{
		if (Type == EArcScalableType::Curve)
		{
			CurvePropProperty->SetValue(SelectedProperty);
		}
		else
		{
			UObject* NullObj = nullptr;
			CurvePropProperty->SetValue(NullObj);
		}
	}
}

#undef LOCTEXT_NAMESPACE
