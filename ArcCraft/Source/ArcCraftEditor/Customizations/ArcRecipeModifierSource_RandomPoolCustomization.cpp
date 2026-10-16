// Copyright Lukasz Baran. All Rights Reserved.

#include "Customizations/ArcRecipeModifierSource_RandomPoolCustomization.h"
#include "Customizations/ArcCraftCustomizationHelpers.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"

TSharedRef<IPropertyTypeCustomization> FArcRecipeModifierSource_RandomPoolCustomization::MakeInstance()
{
	return MakeShareable(new FArcRecipeModifierSource_RandomPoolCustomization());
}

void FArcRecipeModifierSource_RandomPoolCustomization::CustomizeHeader(
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

void FArcRecipeModifierSource_RandomPoolCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	// Groups: Selection / Eligibility / Source
	// Category strings on FArcRecipeModifierSource: "Selection", "Eligibility"
	// Category strings on FArcRecipeModifierSource_RandomPool: "Source", "Selection"
	static const TArray<FName> Groups = {
		FName(TEXT("Selection")),
		FName(TEXT("Eligibility")),
		FName(TEXT("Source"))
	};
	ArcCraftCustomization::GroupChildrenByCategory(StructPropertyHandle, StructBuilder, Groups);
}
