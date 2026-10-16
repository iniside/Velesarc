// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailGroup.h"
#include "PropertyHandle.h"

/**
 * Shared helper for ArcCraft property type customizations.
 *
 * All customizations group top-level children by the first segment of their UPROPERTY
 * Category string (the part before the first '|'). Children whose category matches a
 * declared group name are placed inside that group; any others fall through to a
 * default uncategorised section.
 *
 * Usage in CustomizeChildren:
 *
 *   ArcCraftCustomization::GroupChildrenByCategory(
 *       StructPropertyHandle, ChildrenBuilder,
 *       { TEXT("Slot"), TEXT("Selection"), TEXT("Eligibility"), TEXT("Source") });
 */
namespace ArcCraftCustomization
{

inline void GroupChildrenByCategory(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& ChildrenBuilder,
	TConstArrayView<FName> GroupNames)
{
	// Build an ordered map from group name → group builder.
	// Use an index map to maintain stable ordering.
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

		// Extract the top-level category segment (before the first '|').
		FString Category = Prop->GetMetaData(TEXT("Category"));
		int32 PipeIdx = INDEX_NONE;
		Category.FindChar(TEXT('|'), PipeIdx);
		FName TopCategory = FName(PipeIdx != INDEX_NONE ? Category.Left(PipeIdx) : Category);

		IDetailGroup** FoundGroup = GroupMap.Find(TopCategory);
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

} // namespace ArcCraftCustomization
