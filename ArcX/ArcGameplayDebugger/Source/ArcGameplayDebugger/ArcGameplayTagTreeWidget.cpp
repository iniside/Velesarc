// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGameplayTagTreeWidget.h"

#include "imgui.h"
#include "GameplayTagsManager.h"

void FArcGameplayTagTreeWidget::Initialize()
{
	RebuildTree();
}

void FArcGameplayTagTreeWidget::Uninitialize()
{
	RootNode.Reset();
	SelectedTags.Reset();
	FilterBuf[0] = '\0';
}

void FArcGameplayTagTreeWidget::RebuildTree()
{
	RootNode = MakeShared<FTagTreeNode>();

	FGameplayTagContainer AllTags;
	UGameplayTagsManager::Get().RequestAllGameplayTags(AllTags, true);

	for (const FGameplayTag& Tag : AllTags)
	{
		FString TagString = Tag.ToString();
		TArray<FString> Parts;
		TagString.ParseIntoArray(Parts, TEXT("."));

		FTagTreeNode* Current = RootNode.Get();
		for (const FString& Part : Parts)
		{
			FName PartName(*Part);
			TSharedPtr<FTagTreeNode>& Child = Current->Children.FindOrAdd(PartName);
			if (!Child.IsValid())
			{
				Child = MakeShared<FTagTreeNode>();
				Child->Name = PartName;
			}
			Current = Child.Get();
		}
		// The final node corresponds to an actual registered tag
		Current->Tag = Tag;
	}
}

bool FArcGameplayTagTreeWidget::NodeMatchesFilter(const FTagTreeNode& Node, const FString& Filter) const
{
	if (Filter.IsEmpty())
	{
		return true;
	}

	// Check if this node's full tag matches
	if (Node.Tag.IsValid())
	{
		if (Node.Tag.ToString().ToLower().Contains(Filter))
		{
			return true;
		}
	}

	// Check node name itself
	if (Node.Name.ToString().ToLower().Contains(Filter))
	{
		return true;
	}

	// Check any descendant
	for (const auto& Pair : Node.Children)
	{
		if (NodeMatchesFilter(*Pair.Value, Filter))
		{
			return true;
		}
	}

	return false;
}

void FArcGameplayTagTreeWidget::DrawNode(const FTagTreeNode& Node, const FString& Filter)
{
	// Sort children by name for stable display
	TArray<FName> SortedKeys;
	Node.Children.GetKeys(SortedKeys);
	SortedKeys.Sort(FNameLexicalLess());

	for (const FName& Key : SortedKeys)
	{
		const FTagTreeNode& Child = *Node.Children[Key];

		if (!NodeMatchesFilter(Child, Filter))
		{
			continue;
		}

		const bool bHasChildren = Child.Children.Num() > 0;
		const bool bIsSelectableTag = Child.Tag.IsValid();

		ImGui::PushID(TCHAR_TO_ANSI(*Key.ToString()));

		if (bHasChildren)
		{
			// Auto-open nodes when filtering to show matches
			ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
			if (!Filter.IsEmpty())
			{
				Flags |= ImGuiTreeNodeFlags_DefaultOpen;
			}

			bool bNodeOpen = ImGui::TreeNodeEx(TCHAR_TO_ANSI(*Key.ToString()), Flags);

			// Selection checkbox on the same line for tags that are both a parent and a registered tag
			if (bIsSelectableTag)
			{
				ImGui::SameLine();
				bool bSelected = SelectedTags.HasTagExact(Child.Tag);
				if (ImGui::Checkbox("##sel", &bSelected))
				{
					if (bSelected)
					{
						SelectedTags.AddTag(Child.Tag);
					}
					else
					{
						SelectedTags.RemoveTag(Child.Tag);
					}
				}
			}

			if (bNodeOpen)
			{
				DrawNode(Child, Filter);
				ImGui::TreePop();
			}
		}
		else
		{
			// Leaf node
			ImGuiTreeNodeFlags LeafFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			ImGui::TreeNodeEx(TCHAR_TO_ANSI(*Key.ToString()), LeafFlags);

			if (bIsSelectableTag)
			{
				ImGui::SameLine();
				bool bSelected = SelectedTags.HasTagExact(Child.Tag);
				if (ImGui::Checkbox("##sel", &bSelected))
				{
					if (bSelected)
					{
						SelectedTags.AddTag(Child.Tag);
					}
					else
					{
						SelectedTags.RemoveTag(Child.Tag);
					}
				}
			}
		}

		ImGui::PopID();
	}
}

void FArcGameplayTagTreeWidget::Draw()
{
	ImGui::Begin("Gameplay Tag Tree", &bShow);

	// Rebuild button
	if (ImGui::Button("Refresh"))
	{
		RebuildTree();
	}
	ImGui::SameLine();

	// Display count of selected tags
	const int32 SelectedCount = SelectedTags.Num();
	if (SelectedCount > 0)
	{
		ImGui::SameLine();
		if (ImGui::Button("Clear Selection"))
		{
			SelectedTags.Reset();
		}
		ImGui::SameLine();
		ImGui::Text("(%d selected)", SelectedCount);
	}

	// Filter input
	ImGui::Separator();
	ImGui::InputText("Filter", FilterBuf, IM_ARRAYSIZE(FilterBuf));

	// Show selected tags summary
	if (SelectedCount > 0)
	{
		ImGui::Separator();
		if (ImGui::TreeNodeEx("Selected Tags", ImGuiTreeNodeFlags_DefaultOpen))
		{
			FGameplayTagContainer TagsCopy = SelectedTags;
			for (const FGameplayTag& Tag : TagsCopy)
			{
				ImGui::BulletText("%s", TCHAR_TO_ANSI(*Tag.ToString()));
				ImGui::SameLine();
				FString RemoveId = FString::Printf(TEXT("X##%s"), *Tag.ToString());
				if (ImGui::SmallButton(TCHAR_TO_ANSI(*RemoveId)))
				{
					SelectedTags.RemoveTag(Tag);
				}
			}
			ImGui::TreePop();
		}
	}

	// Tag tree
	ImGui::Separator();
	if (RootNode.IsValid())
	{
		FString Filter = FString(ANSI_TO_TCHAR(FilterBuf)).ToLower();
		DrawNode(*RootNode, Filter);
	}
	else
	{
		ImGui::TextDisabled("No tags loaded. Click Refresh.");
	}

	ImGui::End();
}
