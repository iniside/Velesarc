// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * Reusable ImGui widget that displays the full gameplay tag hierarchy as a tree.
 * Supports string filtering and multi-selection of tags.
 */
class FArcGameplayTagTreeWidget
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	/** Get currently selected tags */
	const FGameplayTagContainer& GetSelectedTags() const { return SelectedTags; }

	bool bShow = false;

private:
	/** Recursive tree node representing a single level in the tag hierarchy */
	struct FTagTreeNode
	{
		FName Name;
		FGameplayTag Tag; // Valid only for leaf/registered tags
		TMap<FName, TSharedPtr<FTagTreeNode>> Children;
	};

	/** Root of the built tag tree */
	TSharedPtr<FTagTreeNode> RootNode;

	/** Filter input buffer */
	char FilterBuf[256] = {};

	/** Currently selected tags */
	FGameplayTagContainer SelectedTags;

	/** Build the full tag tree from the GameplayTagManager */
	void RebuildTree();

	/** Check if a node or any descendant matches the filter */
	bool NodeMatchesFilter(const FTagTreeNode& Node, const FString& Filter) const;

	/** Draw a single tree node recursively */
	void DrawNode(const FTagTreeNode& Node, const FString& Filter);
};
