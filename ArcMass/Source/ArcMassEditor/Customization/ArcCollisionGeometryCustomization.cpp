// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCollisionGeometryCustomization.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "UObject/UnrealType.h"

TSharedRef<IPropertyTypeCustomization> FArcCollisionGeometryCustomization::MakeInstance()
{
	return MakeShared<FArcCollisionGeometryCustomization>();
}

void FArcCollisionGeometryCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	HeaderRow.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	];
}

void FArcCollisionGeometryCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedPtr<IPropertyHandle> AggGeomHandle = StructPropertyHandle->GetChildHandle(TEXT("AggGeom"));
	if (!AggGeomHandle.IsValid())
	{
		return;
	}

	uint32 NumChildren = 0;
	AggGeomHandle->GetNumChildren(NumChildren);

	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = AggGeomHandle->GetChildHandle(ChildIndex);
		if (!ChildHandle.IsValid())
		{
			continue;
		}

		FProperty* ChildProperty = ChildHandle->GetProperty();
		if (ChildProperty && ChildProperty->HasAnyPropertyFlags(CPF_EditFixedSize))
		{
			ChildProperty->ClearPropertyFlags(CPF_EditFixedSize);
		}

		StructBuilder.AddProperty(ChildHandle.ToSharedRef());
	}
}
