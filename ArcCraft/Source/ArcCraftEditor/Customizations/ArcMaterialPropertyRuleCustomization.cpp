// Copyright Lukasz Baran. All Rights Reserved.

#include "Customizations/ArcMaterialPropertyRuleCustomization.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailGroup.h"
#include "PropertyHandle.h"

// FArcMaterialPropertyRule UPROPERTY categories all share the "Rule" top-level prefix:
//   RuleName          -> "Rule"
//   TagQuery          -> "Rule|Matching"
//   RequiredRecipeTags-> "Rule|Matching"
//   MinAverageR       -> "Rule|Matching"
//   Weight            -> "Rule|Weight"
//   WeightModifiers   -> "Rule|Weight"
//   RWeightScaling    -> "Rule|Weight"
//   Modifiers         -> "Rule|Modifiers"
//
// We group by the SECOND segment (after the first '|').
// Properties with no second segment (plain "Rule") are placed in Matching as general info.

namespace ArcMaterialPropertyRuleCustomizationImpl
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

		FName GroupKey;
		if (PipeIdx != INDEX_NONE)
		{
			GroupKey = FName(Category.Mid(PipeIdx + 1));
		}
		else
		{
			// No second segment — fall into first declared group
			GroupKey = (GroupNames.Num() > 0) ? GroupNames[0] : NAME_None;
		}

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

} // namespace ArcMaterialPropertyRuleCustomizationImpl

TSharedRef<IPropertyTypeCustomization> FArcMaterialPropertyRuleCustomization::MakeInstance()
{
	return MakeShareable(new FArcMaterialPropertyRuleCustomization());
}

void FArcMaterialPropertyRuleCustomization::CustomizeHeader(
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

void FArcMaterialPropertyRuleCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	// Groups: Matching (TagQuery + RequiredRecipeTags + MinAverageR + RuleName) / Weight / Modifiers
	static const TArray<FName> Groups = {
		FName(TEXT("Matching")),
		FName(TEXT("Weight")),
		FName(TEXT("Modifiers"))
	};
	ArcMaterialPropertyRuleCustomizationImpl::GroupBySecondSegment(
		StructPropertyHandle, StructBuilder, Groups);
}
