/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "RecipeGraph/ArcRecipeGraphNode.h"

#include "EdGraph/EdGraphPin.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcRecipeGraphNode)

// ---------------------------------------------------------------------------
// File-scoped helper namespace (unique across the project)
// ---------------------------------------------------------------------------

namespace ArcRecipeGraphNodeInternal
{
	/** Shared pin category name used for all dependency connections. */
	static const FName PinCategoryDependency(TEXT("Dependency"));

	/** Name for the single input pin. */
	static const FName PinNameIn(TEXT("In"));

	/** Name for the single output pin. */
	static const FName PinNameOut(TEXT("Out"));

} // namespace ArcRecipeGraphNodeInternal

// ---------------------------------------------------------------------------
// UEdGraphNode overrides
// ---------------------------------------------------------------------------

void UArcRecipeGraphNode::AllocateDefaultPins()
{
	// Recipe nodes: one input pin (requirements) + one output pin (product).
	// RawItem / TagGroup nodes: output pin only (they are sources).
	if (NodeType == EArcRecipeGraphNodeType::Recipe)
	{
		CreatePin(
			EGPD_Input,
			ArcRecipeGraphNodeInternal::PinCategoryDependency,
			ArcRecipeGraphNodeInternal::PinNameIn);
	}

	CreatePin(
		EGPD_Output,
		ArcRecipeGraphNodeInternal::PinCategoryDependency,
		ArcRecipeGraphNodeInternal::PinNameOut);
}

FText UArcRecipeGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return DisplayTitle;
}

FText UArcRecipeGraphNode::GetTooltipText() const
{
	if (NodeType == EArcRecipeGraphNodeType::TagGroup)
	{
		// List the group tags and the number of raw member items.
		FString TagList;
		for (const FGameplayTag& Tag : GroupTags)
		{
			if (!TagList.IsEmpty())
			{
				TagList += TEXT(", ");
			}
			TagList += Tag.ToString();
		}
		return FText::Format(
			NSLOCTEXT("ArcRecipeGraph", "TagGroupTooltip", "Tags: {0}\nRaw members: {1}"),
			FText::FromString(TagList),
			FText::AsNumber(GroupMembers.Num()));
	}

	return FText::FromString(AssetId.ToString());
}

FLinearColor UArcRecipeGraphNode::GetNodeTitleColor() const
{
	switch (NodeType)
	{
		case EArcRecipeGraphNodeType::Recipe:
			return FLinearColor(FColor(72, 191, 145));

		case EArcRecipeGraphNodeType::RawItem:
			return FLinearColor(FColor(230, 145, 60));

		case EArcRecipeGraphNodeType::TagGroup:
			return FLinearColor(FColor(100, 120, 220));

		default:
			break;
	}

	return FLinearColor::White;
}

// ---------------------------------------------------------------------------
// Pin accessors
// ---------------------------------------------------------------------------

UEdGraphPin* UArcRecipeGraphNode::GetInputPin() const
{
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin != nullptr && Pin->Direction == EGPD_Input)
		{
			return Pin;
		}
	}
	return nullptr;
}

UEdGraphPin* UArcRecipeGraphNode::GetOutputPin() const
{
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin != nullptr && Pin->Direction == EGPD_Output)
		{
			return Pin;
		}
	}
	return nullptr;
}
