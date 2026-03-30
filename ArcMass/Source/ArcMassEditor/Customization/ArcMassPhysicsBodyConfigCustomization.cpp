// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassPhysicsBodyConfigCustomization.h"

#include "ArcBodyInstanceExpander.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "ArcMass/Physics/ArcMassPhysicsBodyConfig.h"

TSharedRef<IPropertyTypeCustomization> FArcMassPhysicsBodyConfigCustomization::MakeInstance()
{
	return MakeShared<FArcMassPhysicsBodyConfigCustomization>();
}

void FArcMassPhysicsBodyConfigCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	HeaderRow.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	];
}

void FArcMassPhysicsBodyConfigCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedPtr<IPropertyHandle> BodyTypeHandle = StructPropertyHandle->GetChildHandle(
		GET_MEMBER_NAME_CHECKED(FArcMassPhysicsBodyConfigFragment, BodyType));
	TSharedPtr<IPropertyHandle> BodySetupHandle = StructPropertyHandle->GetChildHandle(
		GET_MEMBER_NAME_CHECKED(FArcMassPhysicsBodyConfigFragment, BodySetup));

	if (BodyTypeHandle.IsValid())
	{
		StructBuilder.AddProperty(BodyTypeHandle.ToSharedRef());
	}
	if (BodySetupHandle.IsValid())
	{
		StructBuilder.AddProperty(BodySetupHandle.ToSharedRef());
	}

	TSharedPtr<IPropertyHandle> BodyTemplateHandle = StructPropertyHandle->GetChildHandle(
		GET_MEMBER_NAME_CHECKED(FArcMassPhysicsBodyConfigFragment, BodyTemplate));
	if (BodyTemplateHandle.IsValid())
	{
		TSharedRef<FArcBodyInstanceExpander> Expander = MakeShared<FArcBodyInstanceExpander>();
		Expander->BuildBodyInstanceUI(BodyTemplateHandle.ToSharedRef(), StructBuilder);
	}
}
