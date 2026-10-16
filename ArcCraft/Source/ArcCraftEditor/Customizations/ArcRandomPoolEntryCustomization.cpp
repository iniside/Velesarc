// Copyright Lukasz Baran. All Rights Reserved.

#include "Customizations/ArcRandomPoolEntryCustomization.h"
#include "Customizations/ArcCraftCustomizationHelpers.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailGroup.h"
#include "PropertyHandle.h"

// FArcRandomPoolEntry UPROPERTY categories:
//   DisplayName            -> "Entry"           (no second segment → ungrouped / general)
//   RequiredIngredientTags -> "Entry|Eligibility"
//   DenyIngredientTags     -> "Entry|Eligibility"
//   MinR    -> "Entry|Eligibility"
//   BaseWeight             -> "Entry|Weight"
//   RWeightScaling   -> "Entry|Weight"
//   WeightModifiers        -> "Entry|Weight"
//   Cost                   -> "Entry|Budget"
//   Modifiers              -> "Entry|Modifiers"
//
// We group by the second segment (after the first '|').
// "Budget" maps to the plan's "Cost" group — we use "Budget" to match the Category string.

namespace ArcRandomPoolEntryCustomizationImpl
{

static void GroupBySecondSegment(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& ChildrenBuilder,
	TConstArrayView<FName> GroupNames)
{
	TMap<FName, IDetailGroup*> GroupMap;
	for (const FName& GroupName : GroupNames)
	{
		IDetailGroup& Group = ChildrenBuilder.AddGroup(GroupName, FText::FromName(GroupName));
		GroupMap.Add(GroupName, &Group);
	}

	uint32 NumChildren = 0;
	StructPropertyHandle->GetNumChildren(NumChildren);

	for (uint32 Idx = 0; Idx < NumChildren; ++Idx)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(Idx);
		if (!ChildHandle.IsValid())
		{
			continue;
		}

		FProperty* Prop = ChildHandle->GetProperty();
		if (!Prop)
		{
			ChildrenBuilder.AddProperty(ChildHandle.ToSharedRef());
			continue;
		}

		FString Category = Prop->GetMetaData(TEXT("Category"));
		int32 PipeIdx = INDEX_NONE;
		Category.FindChar(TEXT('|'), PipeIdx);

		if (PipeIdx == INDEX_NONE)
		{
			// No sub-category — add ungrouped (DisplayName, etc.)
			ChildrenBuilder.AddProperty(ChildHandle.ToSharedRef());
			continue;
		}

		FName GroupKey = FName(Category.Mid(PipeIdx + 1));

		IDetailGroup** FoundGroup = GroupMap.Find(GroupKey);
		if (FoundGroup)
		{
			(*FoundGroup)->AddPropertyRow(ChildHandle.ToSharedRef());
		}
		else
		{
			ChildrenBuilder.AddProperty(ChildHandle.ToSharedRef());
		}
	}
}

} // namespace ArcRandomPoolEntryCustomizationImpl

TSharedRef<IPropertyTypeCustomization> FArcRandomPoolEntryCustomization::MakeInstance()
{
	return MakeShareable(new FArcRandomPoolEntryCustomization());
}

void FArcRandomPoolEntryCustomization::CustomizeHeader(
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

void FArcRandomPoolEntryCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	// Groups derived from the second Category segment:
	//   Eligibility, Weight, Budget (plan calls it "Cost"), Modifiers
	static const TArray<FName> Groups = {
		FName(TEXT("Eligibility")),
		FName(TEXT("Weight")),
		FName(TEXT("Budget")),
		FName(TEXT("Modifiers"))
	};
	ArcRandomPoolEntryCustomizationImpl::GroupBySecondSegment(
		StructPropertyHandle, StructBuilder, Groups);
}
