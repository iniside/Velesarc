// Copyright Lukasz Baran. All Rights Reserved.

#include "Customizations/ArcRecipeModifierSource_MaterialPropertiesCustomization.h"
#include "Customizations/ArcCraftCustomizationHelpers.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"

TSharedRef<IPropertyTypeCustomization> FArcRecipeModifierSource_MaterialPropertiesCustomization::MakeInstance()
{
	return MakeShareable(new FArcRecipeModifierSource_MaterialPropertiesCustomization());
}

void FArcRecipeModifierSource_MaterialPropertiesCustomization::CustomizeHeader(
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

void FArcRecipeModifierSource_MaterialPropertiesCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	// Groups: Source / Remedies / Eligibility
	// Category strings on FArcRecipeModifierSource: "Selection", "Eligibility"
	// Category strings on FArcRecipeModifierSource_MaterialProperties: "Source", "Remedies"
	static const TArray<FName> Groups = {
		FName(TEXT("Source")),
		FName(TEXT("Remedies")),
		FName(TEXT("Eligibility"))
	};
	ArcCraftCustomization::GroupChildrenByCategory(StructPropertyHandle, StructBuilder, Groups);
}
