// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassItemsDebugger.h"
#include "ArcMassItemsDebuggerShared.h"

#include "Commands/ArcMassEquipFromDefinitionCommand.h"
#include "Commands/ArcMassEquipFromStoreCommand.h"
#include "Commands/ArcMassUnequipCommand.h"
#include "Commands/ArcReplicatedCommandHelpers.h"
#include "Equipment/ArcMassEquipmentFragment.h"
#include "Equipment/ArcMassEquipmentSlotPreset.h"
#include "Equipment/ArcMassEquipmentTypes.h"
#include "Equipment/ArcMassEquipmentUtils.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemSpec.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "imgui.h"

namespace ArcMassItemsDebugger::Equipment
{
	static const ImVec4 ColorOk(0.4f, 1.0f, 0.4f, 1.0f);
	static const ImVec4 ColorDim(0.6f, 0.6f, 0.6f, 1.0f);
	static const ImVec4 ColorWarn(1.0f, 0.3f, 0.3f, 1.0f);
	static const ImVec4 ColorYellow(1.0f, 1.0f, 0.3f, 1.0f);

	struct FSlotUiState
	{
		int32 DefinitionIndex = INDEX_NONE;
		int32 StoreItemIndex = INDEX_NONE;
		int32 Mode = 0; // 0 = From Definition, 1 = From Store
	};

	// Per-slot transient UI state, keyed by SlotId.GetTagName().
	static TMap<FName, FSlotUiState> SlotStateByTag;

	FSlotUiState& GetSlotState(const FGameplayTag& SlotId)
	{
		return SlotStateByTag.FindOrAdd(SlotId.GetTagName());
	}

	const FArcItemData* FindItemInStore(const FArcMassItemStoreFragment* Store, const FArcItemId& ItemId)
	{
		if (Store == nullptr || ItemId.IsValid() == false)
		{
			return nullptr;
		}
		for (const FArcMassReplicatedItem& Replicated : Store->ReplicatedItems.Items)
		{
			const FArcItemData* Data = Replicated.ToItem();
			if (Data != nullptr && Data->GetItemId() == ItemId)
			{
				return Data;
			}
		}
		return nullptr;
	}
}

void FArcMassItemsDebugger::DrawEquipmentTab(FMassEntityManager& Manager, FMassEntityHandle Entity)
{
#if WITH_MASSENTITY_DEBUG
	using namespace ArcMassItemsDebugger::Equipment;

	FMassEntityView View(Manager, Entity);
	const FArcMassEquipmentSharedFragment* SharedFrag =
		View.GetConstSharedFragmentDataPtr<FArcMassEquipmentSharedFragment>();
	FArcMassEquipmentFragment* EquipFrag =
		View.GetSparseFragmentDataPtr<FArcMassEquipmentFragment>();

	if (SharedFrag == nullptr || EquipFrag == nullptr)
	{
		ImGui::TextColored(ColorDim, "Entity has no FArcMassEquipmentFragment / SharedFragment.");
		return;
	}

	const UArcMassEquipmentSlotPreset* Preset = SharedFrag->Preset;
	if (Preset == nullptr)
	{
		ImGui::TextColored(ColorWarn, "Equipment SharedFragment has no Preset asset.");
		return;
	}

	FArcMassItemStoreFragment* Store = ArcMassItemsDebugger::GetStore(Manager, Entity);
	UScriptStruct* ResolvedStoreType = FArcMassItemStoreFragment::StaticStruct();

	ImGui::Text("Equipment Preset: %s", TCHAR_TO_ANSI(*GetNameSafe(Preset)));
	if (SharedFrag->Defaults != nullptr)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("(Defaults: %s)", TCHAR_TO_ANSI(*GetNameSafe(SharedFrag->Defaults)));
	}
	ImGui::SameLine();
	ImGui::TextDisabled("Slots: %d  |  Equipped: %d",
		Preset->EquipmentSlots.Num(), EquipFrag->EquippedItems.Num());
	ImGui::Separator();

	for (int32 SlotIndex = 0; SlotIndex < Preset->EquipmentSlots.Num(); ++SlotIndex)
	{
		const FArcMassEquipmentSlot& Slot = Preset->EquipmentSlots[SlotIndex];
		const FString SlotLabel = FString::Printf(TEXT("[Slot] %s"),
			*ArcMassItemsDebugger::FormatGameplayTag(Slot.SlotId));

		ImGui::PushID(SlotIndex);

		const FArcItemId* CurrentlyEquipped = EquipFrag->EquippedItems.Find(Slot.SlotId);
		const bool bSlotOccupied = CurrentlyEquipped != nullptr && CurrentlyEquipped->IsValid();

		const FString TreeLabel = bSlotOccupied
			? SlotLabel
			: FString::Printf(TEXT("%s  (empty)"), *SlotLabel);
		const ImVec4& HeaderColor = bSlotOccupied ? ColorOk : ColorDim;

		ImGui::PushStyleColor(ImGuiCol_Text, HeaderColor);
		const bool bSlotOpen = ImGui::TreeNode("SlotDetails", "%s", TCHAR_TO_ANSI(*TreeLabel));
		ImGui::PopStyleColor();

		if (bSlotOpen)
		{
			ArcMassItemsDebugger::DrawTags("Required Tags", Slot.RequiredTags);
			ArcMassItemsDebugger::DrawTags("Ignore Tags", Slot.IgnoreTags);
			ImGui::Text("Availability Conditions: %d", Slot.AvailabilityConditions.Num());
			const bool bIsAvailable = Arc::MassEquipment::IsSlotAvailable(Manager, Entity, Slot);
			ImGui::Text("Available: %s", bIsAvailable ? "yes" : "no");

			ImGui::Spacing();
			ImGui::SeparatorText("Currently Equipped");
			if (bSlotOccupied)
			{
				const FArcItemData* EquippedData = FindItemInStore(Store, *CurrentlyEquipped);
				ImGui::Text("ItemId: %s",
					TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatItemId(*CurrentlyEquipped)));
				if (EquippedData != nullptr)
				{
					ImGui::Text("Definition: %s",
						TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatItemDefinition(EquippedData->GetItemDefinitionId())));
					ImGui::Text("ItemSlot: %s",
						TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatGameplayTag(EquippedData->GetSlotId())));
					ImGui::Text("Stacks: %d", EquippedData->GetStacks());
				}
				else
				{
					ImGui::TextColored(ColorWarn, "(item id not found in store)");
				}

				if (ImGui::Button("Unequip"))
				{
					UWorld* World = ArcMassItemsDebugger::GetDebugWorld();
					if (World != nullptr)
					{
						Arcx::SendServerCommand<FArcMassUnequipCommand>(World, Entity, Slot.SlotId);
					}
					RefreshEntityList();
				}
			}
			else
			{
				ImGui::TextDisabled("(empty)");
			}

			ImGui::Spacing();
			ImGui::SeparatorText("Equip");

			FSlotUiState& UiState = GetSlotState(Slot.SlotId);
			ImGui::RadioButton("From Definition", &UiState.Mode, 0);
			ImGui::SameLine();
			ImGui::RadioButton("From Store", &UiState.Mode, 1);

			const FGameplayTag& ItemSlotTag = Slot.SlotId;

			if (UiState.Mode == 0)
			{
				if (ImGui::BeginListBox("##EquipDefinitionList", ImVec2(-FLT_MIN, 120.f)))
				{
					for (int32 Index = 0; Index < CachedItemDefinitionNames.Num(); ++Index)
					{
						if (ImGui::Selectable(TCHAR_TO_ANSI(*CachedItemDefinitionNames[Index]),
							UiState.DefinitionIndex == Index))
						{
							UiState.DefinitionIndex = Index;
						}
					}
					ImGui::EndListBox();
				}

				const bool bDefinitionValid = CachedItemDefinitions.IsValidIndex(UiState.DefinitionIndex);
				if (ImGui::Button("Equip From Definition") && bDefinitionValid && ItemSlotTag.IsValid())
				{
					UWorld* World = ArcMassItemsDebugger::GetDebugWorld();
					if (World != nullptr)
					{
						Arcx::SendServerCommand<FArcMassEquipFromDefinitionCommand>(World, Entity, FArcItemSpec::NewItem(CachedItemDefinitions[UiState.DefinitionIndex], 1, 1), ItemSlotTag, Slot.SlotId);
					}
					RefreshEntityList();
				}
				if (!bDefinitionValid)
				{
					ImGui::SameLine();
					ImGui::TextColored(ColorDim, "(select a definition)");
				}
			}
			else
			{
				if (Store == nullptr)
				{
					ImGui::TextColored(ColorWarn, "Entity has no item store.");
				}
				else
				{
					if (ImGui::BeginListBox("##EquipStoreItemList", ImVec2(-FLT_MIN, 120.f)))
					{
						for (int32 Index = 0; Index < Store->ReplicatedItems.Items.Num(); ++Index)
						{
							const FArcItemData* Item = Store->ReplicatedItems.Items[Index].ToItem();
							if (Item == nullptr)
							{
								continue;
							}
							if (Arc::MassEquipment::CanEquip(Manager, Entity, ResolvedStoreType,
								Item->GetItemId(), Slot.SlotId) == false)
							{
								continue;
							}

							const FString Label = FString::Printf(TEXT("%s  [%s]"),
								*ArcMassItemsDebugger::FormatItemDefinition(Item->GetItemDefinitionId()),
								*ArcMassItemsDebugger::FormatItemId(Item->GetItemId()));
							if (ImGui::Selectable(TCHAR_TO_ANSI(*Label),
								UiState.StoreItemIndex == Index))
							{
								UiState.StoreItemIndex = Index;
							}
						}
						ImGui::EndListBox();
					}

					const bool bStoreItemValid =
						Store->ReplicatedItems.Items.IsValidIndex(UiState.StoreItemIndex);
					const FArcItemData* SelectedItem = bStoreItemValid
						? Store->ReplicatedItems.Items[UiState.StoreItemIndex].ToItem()
						: nullptr;

					if (ImGui::Button("Equip From Store") && SelectedItem != nullptr && ItemSlotTag.IsValid())
					{
						UWorld* World = ArcMassItemsDebugger::GetDebugWorld();
						if (World != nullptr)
						{
							Arcx::SendServerCommand<FArcMassEquipFromStoreCommand>(World, Entity, SelectedItem->GetItemId(), ItemSlotTag, Slot.SlotId);
						}
						RefreshEntityList();
					}
					if (SelectedItem == nullptr)
					{
						ImGui::SameLine();
						ImGui::TextColored(ColorDim, "(select a compatible item)");
					}
				}
			}

			ImGui::TreePop();
		}

		ImGui::PopID();
		ImGui::Separator();
	}
#endif
}
