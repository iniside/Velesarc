// Copyright Lukasz Baran. All Rights Reserved.

#include "Customizations/ArcRecipeModifierSource_StaticCustomization.h"
#include "Customizations/ArcCraftCustomizationHelpers.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"

TSharedRef<IPropertyTypeCustomization> FArcRecipeModifierSource_StaticCustomization::MakeInstance()
{
	return MakeShareable(new FArcRecipeModifierSource_StaticCustomization());
}

void FArcRecipeModifierSource_StaticCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	HeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		StructPropertyHandle->CreatePropertyValueWidget()
	];
}

void FArcRecipeModifierSource_StaticCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	// Groups: Source / Eligibility
	// Category strings on FArcRecipeModifierSource: "Selection", "Eligibility"
	// Category strings on FArcRecipeModifierSource_Static: "Source"
	static const TArray<FName> Groups = {
		FName(TEXT("Source")),
		FName(TEXT("Eligibility"))
	};
	ArcCraftCustomization::GroupChildrenByCategory(StructPropertyHandle, StructBuilder, Groups);
}
