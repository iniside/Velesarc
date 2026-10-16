// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassItemsDebugger.h"
#include "ArcMassItemsDebuggerShared.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Commands/ArcMassAddItemCommand.h"
#include "Commands/ArcMassAssignItemSlotCommand.h"
#include "Commands/ArcMassRemoveItemCommand.h"
#include "Commands/ArcMassRemoveItemFromSlotCommand.h"
#include "Commands/ArcReplicatedCommandHelpers.h"
#include "Fragments/ArcMassItemEventFragment.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinitionData.h"
#include "Core/ArcCoreAssetManager.h"
#include "Items/ArcItemInstance.h"
#include "Items/ArcItemSpec.h"
#include "Mass/Debugger.h"
#include "MassDebuggerWorld.h"
#include "Mass/EntityView.h"
#include "StructUtils/InstancedStruct.h"
#include "imgui.h"

namespace
{
	const char* ToEventTypeName(EArcMassItemEventType Type)
	{
		switch (Type)
		{
		case EArcMassItemEventType::Added: return "Added";
		case EArcMassItemEventType::Removed: return "Removed";
		case EArcMassItemEventType::SlotAssigned: return "SlotAssigned";
		case EArcMassItemEventType::SlotCleared: return "SlotCleared";
		case EArcMassItemEventType::SlotChanged: return "SlotChanged(move)";
		case EArcMassItemEventType::StacksChanged: return "StacksChanged";
		case EArcMassItemEventType::Attached: return "Attached";
		case EArcMassItemEventType::Detached: return "Detached";
		default: return "Unknown";
		}
	}
}

void FArcMassItemsDebugger::DrawAddItemPanel(FMassEntityManager& Manager, FMassEntityHandle Entity)
{
	if (ImGui::Button("Quick Add"))
	{
		AddItemMode = AddItemMode == EArcMassAddItemMode::QuickAdd ? EArcMassAddItemMode::None : EArcMassAddItemMode::QuickAdd;
		ItemSpecCreator.Reset();
	}
	ImGui::SameLine();
	if (ImGui::Button("Add Custom"))
	{
		AddItemMode = AddItemMode == EArcMassAddItemMode::CustomSpec ? EArcMassAddItemMode::None : EArcMassAddItemMode::CustomSpec;
		ItemSpecCreator.Reset();
	}
	ImGui::SameLine();
	if (ImGui::Button("Refresh Definitions"))
	{
		RefreshItemDefinitions();
		ItemDefinitionFilterBuf[0] = '\0';
	}

	if (AddItemMode == EArcMassAddItemMode::None)
	{
		return;
	}

	ImGui::SeparatorText(AddItemMode == EArcMassAddItemMode::CustomSpec ? "Add Item (Custom Spec)" : "Add Item (Quick)");

	if (AddItemMode == EArcMassAddItemMode::CustomSpec)
	{
		FArcItemSpec& ResultSpec = ItemSpecCreator.GetResultSpec();
		if (ResultSpec.GetItemDefinitionId().IsValid())
		{
			ItemSpecCreator.Draw();
			if (ItemSpecCreator.IsFinished())
			{
				UWorld* World = ArcMassItemsDebugger::GetDebugWorld();
				if (World != nullptr)
				{
					Arcx::SendServerCommand<FArcMassAddItemCommand>(World, Entity, ItemSpecCreator.GetResultSpec());
				}
				ItemSpecCreator.Reset();
				AddItemMode = EArcMassAddItemMode::None;
				RefreshEntityList();
			}
			else if (ItemSpecCreator.WasCancelled())
			{
				ItemSpecCreator.Reset();
			}
			return;
		}
	}

	ImGui::SetNextItemWidth(220.f);
	ImGui::InputText("Filter##MassItemDefFilter", ItemDefinitionFilterBuf, IM_ARRAYSIZE(ItemDefinitionFilterBuf));

	const FString Filter = FString(UTF8_TO_TCHAR(ItemDefinitionFilterBuf)).ToLower();
	if (ImGui::BeginListBox("##MassItemDefinitionList", ImVec2(-FLT_MIN, 150.f)))
	{
		for (int32 Index = 0; Index < CachedItemDefinitionNames.Num(); ++Index)
		{
			if (!Filter.IsEmpty() && !CachedItemDefinitionNames[Index].ToLower().Contains(Filter))
			{
				continue;
			}

			if (ImGui::Selectable(TCHAR_TO_ANSI(*CachedItemDefinitionNames[Index]), SelectedDefinitionIndex == Index))
			{
				SelectedDefinitionIndex = Index;
			}
		}
		ImGui::EndListBox();
	}

	if (AddItemMode == EArcMassAddItemMode::QuickAdd)
	{
		if (ImGui::Button("Add Selected") && CachedItemDefinitions.IsValidIndex(SelectedDefinitionIndex))
		{
			UWorld* World = ArcMassItemsDebugger::GetDebugWorld();
			if (World != nullptr)
			{
				Arcx::SendServerCommand<FArcMassAddItemCommand>(World, Entity, FArcItemSpec::NewItem(CachedItemDefinitions[SelectedDefinitionIndex], 1, 1));
			}
			RefreshEntityList();
		}
	}
	else if (AddItemMode == EArcMassAddItemMode::CustomSpec)
	{
		if (ImGui::Button("Edit Spec...") && CachedItemDefinitions.IsValidIndex(SelectedDefinitionIndex))
		{
			ItemSpecCreator.Begin(CachedItemDefinitions[SelectedDefinitionIndex]);
		}
	}

	ImGui::SameLine();
	if (ImGui::Button("Close##MassItemsAddPanel"))
	{
		AddItemMode = EArcMassAddItemMode::None;
		ItemSpecCreator.Reset();
	}
}

void FArcMassItemsDebugger::DrawItemsTable(FMassEntityManager& Manager, FMassEntityHandle Entity, FArcMassItemStoreFragment& Store)
{
	ImGui::Text("Items: %d", Store.ReplicatedItems.Items.Num());
	ImGui::SameLine();
	{
		int32 SlottedCount = 0;
		for (const FArcMassReplicatedItem& ReplicatedItem : Store.ReplicatedItems.Items)
		{
			const FArcItemData* ItemData = ReplicatedItem.ToItem();
			if (ItemData && ItemData->GetSlotId().IsValid())
			{
				++SlottedCount;
			}
		}
		ImGui::TextDisabled("Slotted: %d", SlottedCount);
	}

	const ImGuiTableFlags TableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
	if (ImGui::BeginTable("MassItemsTable", 7, TableFlags, ImVec2(0.f, 0.f)))
	{
		ImGui::TableSetupColumn("Definition", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("ItemId", ImGuiTableColumnFlags_WidthFixed, 175.f);
		ImGui::TableSetupColumn("Stacks", ImGuiTableColumnFlags_WidthFixed, 55.f);
		ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, 45.f);
		ImGui::TableSetupColumn("Slot", ImGuiTableColumnFlags_WidthFixed, 150.f);
		ImGui::TableSetupColumn("Version", ImGuiTableColumnFlags_WidthFixed, 60.f);
		ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 90.f);
		ImGui::TableHeadersRow();

		for (int32 Index = 0; Index < Store.ReplicatedItems.Items.Num(); ++Index)
		{
			const FArcItemData* Item = Store.ReplicatedItems.Items[Index].ToItem();
			if (!Item)
			{
				continue;
			}

			ImGui::TableNextRow();
			ImGui::PushID(Index);

			ImGui::TableSetColumnIndex(0);
			if (ImGui::Selectable(TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatItemDefinition(Item->GetItemDefinitionId())), SelectedItemIndex == Index, ImGuiSelectableFlags_SpanAllColumns))
			{
				SelectedItemIndex = Index;
			}

			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%s", TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatItemId(Item->GetItemId())));

			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%d", Item->GetStacks());

			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%d", Item->GetLevel());

			ImGui::TableSetColumnIndex(4);
			ImGui::Text("%s", TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatGameplayTag(Item->GetSlotId())));

			ImGui::TableSetColumnIndex(5);
			ImGui::Text("%u", Item->GetVersion());

			ImGui::TableSetColumnIndex(6);
			if (ImGui::SmallButton("Remove"))
			{
				UWorld* World = ArcMassItemsDebugger::GetDebugWorld();
				if (World != nullptr)
				{
					Arcx::SendServerCommand<FArcMassRemoveItemCommand>(World, Entity, Item->GetItemId());
				}
				if (SelectedItemIndex >= Store.ReplicatedItems.Items.Num() - 1)
				{
					SelectedItemIndex = Store.ReplicatedItems.Items.Num() - 2;
				}
				RefreshEntityList();
			}

			ImGui::PopID();
		}

		ImGui::EndTable();
	}
}

void FArcMassItemsDebugger::DrawSelectedItemDetail(FMassEntityManager& Manager, FMassEntityHandle Entity, FArcMassItemStoreFragment& Store)
{
	if (!Store.ReplicatedItems.Items.IsValidIndex(SelectedItemIndex))
	{
		ImGui::TextDisabled("Select an item to inspect");
		return;
	}

	const FArcItemData* Item = ArcMassItemsDebugger::GetItemAt(Store, SelectedItemIndex);
	if (!Item)
	{
		ImGui::TextDisabled("Selected item has invalid item data");
		return;
	}

	DrawItemDetail(Manager, Entity, *Item);
}

void FArcMassItemsDebugger::DrawItemDetail(FMassEntityManager& Manager, FMassEntityHandle Entity, const FArcItemData& Item)
{
	const FArcItemDefinitionData* Definition = Item.GetItemDefinition();

	if (ImGui::BeginTabBar("MassItemDetailTabs"))
	{
		if (ImGui::BeginTabItem("Overview"))
		{
			ImGui::Text("Definition: %s", TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatItemDefinition(Item.GetItemDefinitionId())));
			ImGui::Text("DefinitionId: %s", TCHAR_TO_ANSI(*Item.GetItemDefinitionId().ToString()));
			ImGui::Text("ItemId: %s", TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatItemId(Item.GetItemId())));
			ImGui::Text("Level: %d", Item.GetLevel());
			ImGui::Text("Stacks: %d", Item.GetStacks());
			ImGui::Text("Version: %u", Item.GetVersion());
			ImGui::Text("Slot: %s", TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatGameplayTag(Item.GetSlotId())));
			ImGui::Text("Attach Slot: %s", TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatGameplayTag(Item.GetAttachSlot())));
			ImGui::Text("OwnerId: %s", TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatItemId(Item.GetOwnerId())));
			ImGui::Text("OldOwnerId: %s", TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatItemId(Item.OldOwnerId)));
			ImGui::Text("OldSlot: %s", TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatGameplayTag(Item.OldSlot)));
			ImGui::Text("OldAttachedToSlot: %s", TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatGameplayTag(Item.OldAttachedToSlot)));
			ImGui::Text("ForceRep: %u", Item.ForceRep);

			ImGui::Spacing();
			ImGui::SeparatorText("Slot Controls");
			ImGui::SetNextItemWidth(260.f);
			ImGui::InputText("Slot Tag##MassItemSlotTag", SlotTagBuf, IM_ARRAYSIZE(SlotTagBuf));
			ImGui::SameLine();
			if (ImGui::Button("Assign Slot"))
			{
				const FString SlotTagText = FString(UTF8_TO_TCHAR(SlotTagBuf));
				if (!SlotTagText.IsEmpty())
				{
					const FGameplayTag SlotTag = FGameplayTag::RequestGameplayTag(FName(*SlotTagText), false);
					if (SlotTag.IsValid())
					{
						UWorld* World = ArcMassItemsDebugger::GetDebugWorld();
						if (World != nullptr)
						{
							Arcx::SendServerCommand<FArcMassAssignItemSlotCommand>(World, Entity, Item.GetItemId(), SlotTag);
						}
					}
				}
			}
			if (Item.GetSlotId().IsValid())
			{
				ImGui::SameLine();
				if (ImGui::Button("Remove Slot"))
				{
					UWorld* World = ArcMassItemsDebugger::GetDebugWorld();
					if (World != nullptr)
					{
						Arcx::SendServerCommand<FArcMassRemoveItemFromSlotCommand>(World, Entity, Item.GetItemId());
					}
				}
			}

			if (Item.AttachedItems.Num() > 0)
			{
				ImGui::Spacing();
				ImGui::SeparatorText("Attached Items");
				for (const FArcItemId& AttachedItemId : Item.AttachedItems)
				{
					ImGui::BulletText("%s", TCHAR_TO_ANSI(*AttachedItemId.ToString()));
				}
			}

			ArcMassItemsDebugger::DrawTags("Dynamic Tags", Item.DynamicTags);
			ArcMassItemsDebugger::DrawTags("Aggregated Tags", Item.GetItemAggregatedTags());

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Spec"))
		{
			ArcMassItemsDebugger::DrawStructProperties(FArcItemSpec::StaticStruct(), &Item.Spec, "MassItemSpec");
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Instances"))
		{
			DrawInstancedDataTree(Item);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Definition"))
		{
			DrawDefinitionFragments(Definition);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Events"))
		{
			FMassEntityView EntityView(Manager, Entity);
			const FArcMassItemEventFragment* EventFragment = EntityView.GetSparseFragmentDataPtr<FArcMassItemEventFragment>();
			if (!EventFragment || EventFragment->Events.Num() == 0)
			{
				ImGui::TextDisabled("No current item events on entity");
			}
			else if (ImGui::BeginTable("MassItemEvents", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
			{
				ImGui::TableSetupColumn("Type");
				ImGui::TableSetupColumn("ItemId");
				ImGui::TableSetupColumn("Slot");
				ImGui::TableSetupColumn("StackDelta");
				ImGui::TableHeadersRow();

				for (const FArcMassItemEvent& Event : EventFragment->Events)
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("%s", ToEventTypeName(Event.Type));
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%s", TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatItemId(Event.ItemId)));
					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%s", TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatGameplayTag(Event.SlotTag)));
					ImGui::TableSetColumnIndex(3);
					ImGui::Text("%d", Event.StackDelta);
				}
				ImGui::EndTable();
			}
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
}

void FArcMassItemsDebugger::DrawInstancedDataTree(const FArcItemData& Item) const
{
	if (Item.InstancedData.Num() == 0 && Item.ItemInstances.Data.Num() == 0)
	{
		ImGui::TextDisabled("(none)");
		return;
	}

	if (Item.InstancedData.Num() > 0)
	{
		ImGui::SeparatorText("Runtime InstancedData");
		for (const TPair<const UScriptStruct*, FStructView>& Pair : Item.InstancedData)
		{
			const UScriptStruct* StructType = Pair.Key;
			const FArcItemInstance* Instance = Pair.Value.GetPtr<FArcItemInstance>();
			if (!StructType || !Instance)
			{
				continue;
			}

			if (ImGui::TreeNode(TCHAR_TO_ANSI(*StructType->GetName())))
			{
				ImGui::TextWrapped("%s", TCHAR_TO_ANSI(*Instance->ToString()));
				ArcMassItemsDebugger::DrawStructProperties(StructType, Instance, "RuntimeInstanceData");
				ImGui::TreePop();
			}
		}
	}

	if (Item.ItemInstances.Data.Num() > 0)
	{
		ImGui::SeparatorText("Replicated ItemInstances");
		for (int32 Index = 0; Index < Item.ItemInstances.Data.Num(); ++Index)
		{
			const FInstancedStruct& InstanceStruct = Item.ItemInstances.Data[Index];
			if (!InstanceStruct.IsValid())
			{
				continue;
			}

			const UScriptStruct* StructType = InstanceStruct.GetScriptStruct();
			const FArcItemInstance* Instance = InstanceStruct.GetPtr<FArcItemInstance>();
			const FString Label = FString::Printf(TEXT("[%d] %s"), Index, *GetNameSafe(StructType));
			if (ImGui::TreeNode(TCHAR_TO_ANSI(*Label)))
			{
				if (Instance)
				{
					ImGui::TextWrapped("%s", TCHAR_TO_ANSI(*Instance->ToString()));
				}
				ArcMassItemsDebugger::DrawStructProperties(StructType, InstanceStruct.GetMemory(), "ReplicatedItemInstance");
				ImGui::TreePop();
			}
		}
	}
}

void FArcMassItemsDebugger::DrawDefinitionFragments(const FArcItemDefinitionData* Definition) const
{
	if (!Definition)
	{
		ImGui::TextDisabled("No item definition loaded");
		return;
	}

	const FPrimaryAssetId DefinitionId = Definition->GetPrimaryAssetId();
	ImGui::Text("File: %s", TCHAR_TO_ANSI(*UArcCoreAssetManager::Get().GetJsonRecordFilename(DefinitionId)));
	ImGui::Text("PrimaryAssetId: %s", TCHAR_TO_ANSI(*DefinitionId.ToString()));

	ImGui::SeparatorText("Fragments");
	const TSet<FArcInstancedStruct>& Fragments = Definition->GetFragmentSet();
	if (Fragments.Num() == 0)
	{
		ImGui::TextDisabled("(none)");
	}
	for (const FArcInstancedStruct& Fragment : Fragments)
	{
		if (!Fragment.IsValid())
		{
			continue;
		}
		if (ImGui::TreeNode(TCHAR_TO_ANSI(*GetNameSafe(Fragment.GetScriptStruct()))))
		{
			ArcMassItemsDebugger::DrawStructProperties(Fragment.GetScriptStruct(), Fragment.GetMemory(), "DefinitionFragment");
			ImGui::TreePop();
		}
	}

	const TSet<FArcInstancedStruct>& ScalableFragments = Definition->GetScalableFloatFragments();
	if (ScalableFragments.Num() > 0)
	{
		ImGui::SeparatorText("Scalable Float Fragments");
		for (const FArcInstancedStruct& Fragment : ScalableFragments)
		{
			if (!Fragment.IsValid())
			{
				continue;
			}
			if (ImGui::TreeNode(TCHAR_TO_ANSI(*GetNameSafe(Fragment.GetScriptStruct()))))
			{
				ArcMassItemsDebugger::DrawStructProperties(Fragment.GetScriptStruct(), Fragment.GetMemory(), "DefinitionScalableFragment");
				ImGui::TreePop();
			}
		}
	}
}

void FArcMassItemsDebugger::DrawItemsTab(FMassEntityManager& Manager, FMassEntityHandle Entity)
{
#if WITH_MASSENTITY_DEBUG
	FArcMassItemStoreFragment* Store = ArcMassItemsDebugger::GetStore(Manager, Entity);
	if (!Store)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
			"Entity has no FArcMassItemStoreFragment.");
		return;
	}

	if (PendingJumpItemId.IsValid())
	{
		SelectedItemIndex = INDEX_NONE;
		for (int32 I = 0; I < Store->ReplicatedItems.Items.Num(); ++I)
		{
			const FArcItemData* Data = Store->ReplicatedItems.Items[I].ToItem();
			if (Data && Data->GetItemId() == PendingJumpItemId)
			{
				SelectedItemIndex = I;
				break;
			}
		}
		PendingJumpItemId = FArcItemId();
	}

	DrawAddItemPanel(Manager, Entity);
	ImGui::Spacing();

	const float TopHeight = ImGui::GetContentRegionAvail().y * 0.42f;
	if (ImGui::BeginChild("MassItemsStoreTop", ImVec2(0, TopHeight),
		ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY))
	{
		DrawItemsTable(Manager, Entity, *Store);
	}
	ImGui::EndChild();

	if (ImGui::BeginChild("MassItemsStoreBottom", ImVec2(0, 0), ImGuiChildFlags_Borders))
	{
		DrawSelectedItemDetail(Manager, Entity, *Store);
	}
	ImGui::EndChild();
#endif
}
