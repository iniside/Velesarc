// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEquipmentDebugger.h"

#include "imgui.h"
#include "Commands/ArcEquipItemCommand.h"
#include "Commands/ArcReplicatedCommandHelpers.h"
#include "Commands/ArcUnequipItemCommand.h"
#include "Equipment/ArcEquipmentComponent.h"
#include "GameFramework/PlayerState.h"
#include "Items/ArcItemsStoreComponent.h"
#include "Kismet/GameplayStatics.h"

void FArcEquipmentDebugger::Initialize()
{
	SelectedSlotIdx = -1;
}

void FArcEquipmentDebugger::Uninitialize()
{
	SelectedSlotIdx = -1;
}

void FArcEquipmentDebugger::Draw()
{
	if (!GEngine || !GEngine->GameViewport)
	{
		return;
	}

	UWorld* World = GEngine->GameViewport->GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC || !PC->PlayerState)
	{
		return;
	}

	UArcEquipmentComponent* EquipmentComponent = PC->PlayerState->FindComponentByClass<UArcEquipmentComponent>();
	if (!EquipmentComponent)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Equipment Debugger", &bShow))
	{
		ImGui::End();
		return;
	}

	DrawOverview(PC, EquipmentComponent);

	ImGui::Separator();

	DrawItemsStoreInfo(EquipmentComponent);

	ImGui::End();
}

void FArcEquipmentDebugger::DrawOverview(APlayerController* PC, UArcEquipmentComponent* EquipmentComponent)
{
	UArcItemsStoreComponent* EquipmentItemsStore = EquipmentComponent->GetItemsStore();
	const TArray<FArcEquipmentSlot>& EquipmentSlots = EquipmentComponent->GetEquipmentSlots();

	// Header info
	const UArcEquipmentSlotPreset* Preset = EquipmentComponent->GetEquipmentSlotPreset();
	if (Preset)
	{
		ImGui::Text("Preset: %s", TCHAR_TO_ANSI(*GetNameSafe(Preset)));
	}
	ImGui::Text("Items Store: %s", TCHAR_TO_ANSI(*GetNameSafe(EquipmentItemsStore)));
	ImGui::Text("Items Store Class: %s", TCHAR_TO_ANSI(*GetNameSafe(EquipmentComponent->GetItemsStoreClass())));
	ImGui::Text("Slots: %d", EquipmentSlots.Num());

	ImGui::Spacing();

	if (EquipmentSlots.Num() == 0)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "No equipment slots configured.");
		return;
	}

	// Equipment slots table
	const ImGuiTableFlags TableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;
	if (ImGui::BeginTable("SlotsTable", 5, TableFlags))
	{
		ImGui::TableSetupColumn("Slot", ImGuiTableColumnFlags_None, 2.0f);
		ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_None, 1.0f);
		ImGui::TableSetupColumn("Equipped Item", ImGuiTableColumnFlags_None, 2.5f);
		ImGui::TableSetupColumn("Equip", ImGuiTableColumnFlags_None, 2.5f);
		ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_None, 1.5f);
		ImGui::TableHeadersRow();

		for (int32 Idx = 0; Idx < EquipmentSlots.Num(); ++Idx)
		{
			const FArcEquipmentSlot& Slot = EquipmentSlots[Idx];
			if (!Slot.SlotId.IsValid())
			{
				continue;
			}

			ImGui::PushID(Idx);
			ImGui::TableNextRow();

			// Slot ID
			ImGui::TableNextColumn();
			FString SlotName = Slot.SlotId.ToString();
			// Strip the common prefix for readability
			SlotName.RemoveFromStart(TEXT("SlotId."));
			bool bSelected = (SelectedSlotIdx == Idx);
			if (ImGui::Selectable(TCHAR_TO_ANSI(*SlotName), bSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
			{
				SelectedSlotIdx = bSelected ? -1 : Idx;
			}

			// Status
			ImGui::TableNextColumn();
			const FArcItemData* SlotItem = EquipmentItemsStore ? EquipmentItemsStore->GetItemFromSlot(Slot.SlotId) : nullptr;
			const bool bIsPending = SlotItem && EquipmentItemsStore->IsPending(SlotItem->GetItemId());
			if (bIsPending)
			{
				ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Pending");
			}
			else
			{
				ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Open");
			}

			// Equipped Item
			ImGui::TableNextColumn();
			const FArcItemData* ExistingItem = EquipmentItemsStore ? EquipmentItemsStore->GetItemFromSlot(Slot.SlotId) : nullptr;
			if (ExistingItem && ExistingItem->GetItemDefinition())
			{
				ImGui::Text("%s", TCHAR_TO_ANSI(*GetNameSafe(ExistingItem->GetItemDefinition())));
			}
			else
			{
				ImGui::TextDisabled("Empty");
			}

			// Equip combo
			ImGui::TableNextColumn();
			FString ComboLabel = FString::Printf(TEXT("##EquipCombo_%d"), Idx);
			FString PreviewText = ExistingItem ? GetNameSafe(ExistingItem->GetItemDefinition()) : TEXT("Select Item...");
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::BeginCombo(TCHAR_TO_ANSI(*ComboLabel), TCHAR_TO_ANSI(*PreviewText)))
			{
				TArray<const FArcItemData*> Items = EquipmentComponent->GetItemsFromStoreForSlot(Slot.SlotId);
				for (const FArcItemData* ItemData : Items)
				{
					if (!ItemData || !ItemData->GetItemDefinition())
					{
						continue;
					}

					ImGui::PushID(TCHAR_TO_ANSI(*ItemData->GetItemId().ToString()));
					FString ItemLabel = FString::Printf(TEXT("%s (x%d L%d)"),
						*GetNameSafe(ItemData->GetItemDefinition()),
						ItemData->GetStacks(),
						ItemData->GetLevel());
					if (ImGui::Selectable(TCHAR_TO_ANSI(*ItemLabel)))
					{
						Arcx::SendServerCommand<FArcEquipItemCommand>(PC,
							EquipmentComponent->GetItemsStore(), EquipmentComponent,
							ItemData->GetItemId(), Slot.SlotId);
					}
					ImGui::PopID();
				}
				ImGui::EndCombo();
			}

			// Actions
			ImGui::TableNextColumn();
			if (ExistingItem)
			{
				FString UnequipLabel = FString::Printf(TEXT("Unequip##%d"), Idx);
				if (ImGui::SmallButton(TCHAR_TO_ANSI(*UnequipLabel)))
				{
					Arcx::SendServerCommand<FArcUnequipItemCommand>(PC, EquipmentComponent, Slot.SlotId);
				}
			}

			ImGui::PopID();
		}

		ImGui::EndTable();
	}

	// Slot details panel
	if (SelectedSlotIdx >= 0 && SelectedSlotIdx < EquipmentSlots.Num())
	{
		ImGui::Spacing();
		DrawSlotDetails(PC, EquipmentComponent, SelectedSlotIdx);
	}
}

void FArcEquipmentDebugger::DrawSlotDetails(APlayerController* PC, UArcEquipmentComponent* EquipmentComponent, int32 SlotIdx)
{
	const TArray<FArcEquipmentSlot>& EquipmentSlots = EquipmentComponent->GetEquipmentSlots();
	const FArcEquipmentSlot& Slot = EquipmentSlots[SlotIdx];
	UArcItemsStoreComponent* EquipmentItemsStore = EquipmentComponent->GetItemsStore();

	FString HeaderText = FString::Printf(TEXT("Slot Details: %s"), *Slot.SlotId.ToString());
	if (ImGui::CollapsingHeader(TCHAR_TO_ANSI(*HeaderText), ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Indent();

		// Slot configuration
		if (ImGui::TreeNodeEx("Slot Configuration", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (Slot.RequiredTags.Num() > 0)
			{
				ImGui::Text("Required Tags: %s", TCHAR_TO_ANSI(*Slot.RequiredTags.ToStringSimple()));
			}
			else
			{
				ImGui::TextDisabled("Required Tags: None");
			}

			if (Slot.IgnoreTags.Num() > 0)
			{
				ImGui::Text("Ignore Tags: %s", TCHAR_TO_ANSI(*Slot.IgnoreTags.ToStringSimple()));
			}
			else
			{
				ImGui::TextDisabled("Ignore Tags: None");
			}

			if (Slot.OwnerRequiredTags.Num() > 0)
			{
				ImGui::Text("Owner Required Tags: %s", TCHAR_TO_ANSI(*Slot.OwnerRequiredTags.ToStringSimple()));
			}

			if (Slot.OwnerIgnoreTags.Num() > 0)
			{
				ImGui::Text("Owner Ignore Tags: %s", TCHAR_TO_ANSI(*Slot.OwnerIgnoreTags.ToStringSimple()));
			}

			if (Slot.CustomCondition.IsValid())
			{
				ImGui::Text("Custom Condition: %s", TCHAR_TO_ANSI(*GetNameSafe(Slot.CustomCondition.GetScriptStruct())));
			}
			else
			{
				ImGui::TextDisabled("Custom Condition: None");
			}

			ImGui::TreePop();
		}

		// Equipped item details
		const FArcItemData* ExistingItem = EquipmentItemsStore ? EquipmentItemsStore->GetItemFromSlot(Slot.SlotId) : nullptr;
		if (ExistingItem && ImGui::TreeNodeEx("Equipped Item", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("Definition: %s", TCHAR_TO_ANSI(*GetNameSafe(ExistingItem->GetItemDefinition())));
			ImGui::Text("ItemId: %s", TCHAR_TO_ANSI(*ExistingItem->GetItemId().ToString()));
			ImGui::Text("Level: %d", ExistingItem->GetLevel());
			ImGui::Text("Stacks: %d", ExistingItem->GetStacks());
			ImGui::Text("Slot: %s", TCHAR_TO_ANSI(*ExistingItem->GetSlotId().ToString()));
			ImGui::Text("Store: %s", TCHAR_TO_ANSI(*GetNameSafe(ExistingItem->GetItemsStoreComponent())));

			if (ExistingItem->GetItemAggregatedTags().Num() > 0)
			{
				ImGui::Text("Aggregated Tags: %s", TCHAR_TO_ANSI(*ExistingItem->GetItemAggregatedTags().ToStringSimple()));
			}

			if (ExistingItem->DynamicTags.Num() > 0)
			{
				ImGui::Text("Dynamic Tags: %s", TCHAR_TO_ANSI(*ExistingItem->DynamicTags.ToStringSimple()));
			}

			if (ExistingItem->GetAttachedItems().Num() > 0)
			{
				if (ImGui::TreeNode("Attachments"))
				{
					for (const FArcItemId& AttachId : ExistingItem->GetAttachedItems())
					{
						const FArcItemData* AttachedItem = EquipmentItemsStore ? EquipmentItemsStore->GetItemPtr(AttachId) : nullptr;
						if (AttachedItem)
						{
							ImGui::BulletText("[%s] %s",
								TCHAR_TO_ANSI(*AttachedItem->GetAttachSlot().ToString()),
								TCHAR_TO_ANSI(*GetNameSafe(AttachedItem->GetItemDefinition())));
						}
						else
						{
							ImGui::BulletText("%s (not resolved)", TCHAR_TO_ANSI(*AttachId.ToString()));
						}
					}
					ImGui::TreePop();
				}
			}

			// Unequip button
			ImGui::Spacing();
			if (ImGui::Button("Unequip"))
			{
				Arcx::SendServerCommand<FArcUnequipItemCommand>(PC, EquipmentComponent, Slot.SlotId);
			}

			ImGui::TreePop();
		}
		else if (!ExistingItem)
		{
			ImGui::TextDisabled("No item equipped in this slot.");
		}

		ImGui::Unindent();
	}
}

void FArcEquipmentDebugger::DrawItemsStoreInfo(UArcEquipmentComponent* EquipmentComponent)
{
	UArcItemsStoreComponent* ItemsStore = EquipmentComponent->GetItemsStore();
	if (!ItemsStore)
	{
		ImGui::TextDisabled("No Items Store available.");
		return;
	}

	FString Header = FString::Printf(TEXT("Backing Store: %s (%d items)"),
		*GetNameSafe(ItemsStore), ItemsStore->GetItemNum());
	if (ImGui::CollapsingHeader(TCHAR_TO_ANSI(*Header)))
	{
		TArray<const FArcItemData*> AllItems = ItemsStore->GetItems();
		if (AllItems.Num() == 0)
		{
			ImGui::TextDisabled("Store is empty.");
			return;
		}

		const ImGuiTableFlags TableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;
		if (ImGui::BeginTable("StoreItemsTable", 5, TableFlags))
		{
			ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_None, 3.0f);
			ImGui::TableSetupColumn("Stacks", ImGuiTableColumnFlags_None, 0.8f);
			ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_None, 0.8f);
			ImGui::TableSetupColumn("Slot", ImGuiTableColumnFlags_None, 2.0f);
			ImGui::TableSetupColumn("Locked", ImGuiTableColumnFlags_None, 0.8f);
			ImGui::TableHeadersRow();

			for (const FArcItemData* Item : AllItems)
			{
				if (!Item)
				{
					continue;
				}

				ImGui::TableNextRow();

				ImGui::TableNextColumn();
				ImGui::Text("%s", TCHAR_TO_ANSI(*GetNameSafe(Item->GetItemDefinition())));

				ImGui::TableNextColumn();
				ImGui::Text("%d", Item->GetStacks());

				ImGui::TableNextColumn();
				ImGui::Text("%d", Item->GetLevel());

				ImGui::TableNextColumn();
				if (Item->GetSlotId().IsValid())
				{
					ImGui::Text("%s", TCHAR_TO_ANSI(*Item->GetSlotId().ToString()));
				}
				else
				{
					ImGui::TextDisabled("-");
				}

				ImGui::TableNextColumn();
				bool bItemLocked = ItemsStore->IsPending(Item->GetItemId());
				if (bItemLocked)
				{
					ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Yes");
				}
				else
				{
					ImGui::TextDisabled("No");
				}
			}

			ImGui::EndTable();
		}
	}
}
