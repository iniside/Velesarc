// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassItemsDebugger.h"
#include "ArcMassItemsDebuggerShared.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Bags/ArcBagDefinition.h"
#include "Bags/ArcBagId.h"
#include "Bags/ArcBagOperations.h"
#include "Bags/ArcBagStoreFragment.h"
#include "Commands/ArcMassAddBagCommand.h"
#include "Commands/ArcMassAddNewItemToBagCommand.h"
#include "Commands/ArcMassClearBagSlotCommand.h"
#include "Commands/ArcMassMoveBagItemCommand.h"
#include "Commands/ArcMassPlaceItemInBagSlotCommand.h"
#include "Commands/ArcMassRemoveBagCommand.h"
#include "Commands/ArcMassReorderBagCommand.h"
#include "Commands/ArcReplicatedCommandHelpers.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinitionData.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "imgui.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcBagsDebugger, Log, All);

namespace ArcMassItemsDebugger::Bags
{
	static const ImVec4 ColorOk(0.4f, 1.0f, 0.4f, 1.0f);
	static const ImVec4 ColorDim(0.6f, 0.6f, 0.6f, 1.0f);
	static const ImVec4 ColorWarn(1.0f, 0.3f, 0.3f, 1.0f);

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

	// Number of occupied (valid item id) slots in a bag.
	int32 CountUsedSlots(const FArcBagInstance& Bag)
	{
		int32 Used = 0;
		for (const FArcItemId& SlotItem : Bag.Slots)
		{
			if (SlotItem.IsValid())
			{
				++Used;
			}
		}
		return Used;
	}
}

void FArcMassItemsDebugger::RefreshBagDefinitions()
{
	CachedBagDefinitions.Reset();
	CachedBagDefinitionNames.Reset();
	SelectedBagDefinitionIndex = INDEX_NONE;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	AssetRegistry.GetAssetsByClass(UArcBagDefinition::StaticClass()->GetClassPathName(), CachedBagDefinitions, true);

	for (const FAssetData& AssetData : CachedBagDefinitions)
	{
		CachedBagDefinitionNames.Add(AssetData.AssetName.ToString());
	}

	if (CachedBagDefinitions.Num() > 0)
	{
		SelectedBagDefinitionIndex = 0;
	}
}

void FArcMassItemsDebugger::DrawBagsTab(FMassEntityManager& Manager, FMassEntityHandle Entity)
{
#if WITH_MASSENTITY_DEBUG
	using namespace ArcMassItemsDebugger::Bags;

	FMassEntityView View(Manager, Entity);
	FArcMassBagStoreFragment* BagStore = View.GetSparseFragmentDataPtr<FArcMassBagStoreFragment>();
	if (BagStore == nullptr)
	{
		ImGui::TextColored(ColorWarn, "Entity has no FArcMassBagStoreFragment.");
		ImGui::TextDisabled("Add a bag store trait (with a target item store) to this entity's template.");
		return;
	}

	ImGui::TextDisabled("Bags are not replicated yet - authoritative only on server/standalone worlds.");
	ImGui::Separator();

	FArcMassItemStoreFragment* Store = ArcMassItemsDebugger::GetStore(Manager, Entity);
	UWorld* World = ArcMassItemsDebugger::GetDebugWorld();

	// --- Add Bag row: picker over the cached UArcBagDefinition asset scan ---
	{
		if (CachedBagDefinitions.Num() == 0)
		{
			RefreshBagDefinitions();
		}

		ImGui::SetNextItemWidth(260.f);
		const FString PreviewName = CachedBagDefinitionNames.IsValidIndex(SelectedBagDefinitionIndex)
			? CachedBagDefinitionNames[SelectedBagDefinitionIndex]
			: FString(TEXT("(select a bag definition)"));
		if (ImGui::BeginCombo("##BagDefinitionPicker", TCHAR_TO_ANSI(*PreviewName)))
		{
			for (int32 Index = 0; Index < CachedBagDefinitionNames.Num(); ++Index)
			{
				if (ImGui::Selectable(TCHAR_TO_ANSI(*CachedBagDefinitionNames[Index]), SelectedBagDefinitionIndex == Index))
				{
					SelectedBagDefinitionIndex = Index;
				}
			}
			ImGui::EndCombo();
		}

		ImGui::SameLine();
		const bool bBagDefValid = CachedBagDefinitions.IsValidIndex(SelectedBagDefinitionIndex);
		ImGui::BeginDisabled(bBagDefValid == false);
		if (ImGui::Button("Add Bag") && bBagDefValid)
		{
			// The command wants a live UArcBagDefinition pointer, so resolve the asset synchronously.
			UArcBagDefinition* BagDef = Cast<UArcBagDefinition>(CachedBagDefinitions[SelectedBagDefinitionIndex].GetAsset());
			if (BagDef != nullptr && World != nullptr)
			{
				Arcx::SendServerCommand<FArcMassAddBagCommand>(World, Entity, BagDef);
			}
		}
		ImGui::EndDisabled();

		ImGui::SameLine();
		if (ImGui::Button("Refresh Definitions##BagDefs"))
		{
			RefreshBagDefinitions();
		}
	}

	ImGui::Separator();

	if (BagStore->Bags.Items.Num() == 0)
	{
		ImGui::TextDisabled("(no bags on this entity)");
	}

	// --- Per bag ---
	for (int32 BagIdx = 0; BagIdx < BagStore->Bags.Items.Num(); ++BagIdx)
	{
		FArcBagInstance& Bag = BagStore->Bags.Items[BagIdx];
		const FArcBagId BagId = Bag.BagId;
		const int32 SlotCount = Bag.Slots.Num();
		const int32 UsedSlots = CountUsedSlots(Bag);
		const bool bBagOccupied = ArcMassItems::Bags::IsBagOccupied(Bag);

		const UArcBagDefinition* Def = ArcMassItems::Bags::ResolveBagDefinition(Bag);
		const FString DisplayName = (Def != nullptr && Def->DisplayName.IsEmpty() == false)
			? Def->DisplayName.ToString()
			: FString(TEXT("<unnamed>"));

		ImGui::PushID(TCHAR_TO_ANSI(*BagId.ToString()));

		const FString HeaderLabel = FString::Printf(TEXT("%s  [%s]  slots:%d used:%d free:%d"),
			*DisplayName, *BagId.ToString(), SlotCount, UsedSlots, SlotCount - UsedSlots);

		if (ImGui::CollapsingHeader(TCHAR_TO_ANSI(*HeaderLabel)))
		{
			// Reorder / remove controls.
			ImGui::BeginDisabled(BagIdx <= 0);
			if (ImGui::SmallButton("Move Up") && World != nullptr)
			{
				Arcx::SendServerCommand<FArcMassReorderBagCommand>(World, Entity, BagId, BagIdx - 1);
			}
			ImGui::EndDisabled();

			ImGui::SameLine();
			ImGui::BeginDisabled(BagIdx >= BagStore->Bags.Items.Num() - 1);
			if (ImGui::SmallButton("Move Down") && World != nullptr)
			{
				Arcx::SendServerCommand<FArcMassReorderBagCommand>(World, Entity, BagId, BagIdx + 1);
			}
			ImGui::EndDisabled();

			ImGui::SameLine();
			ImGui::BeginDisabled(bBagOccupied);
			if (ImGui::SmallButton("Remove Bag") && World != nullptr)
			{
				Arcx::SendServerCommand<FArcMassRemoveBagCommand>(World, Entity, BagId);
			}
			ImGui::EndDisabled();
			if (bBagOccupied)
			{
				ImGui::SameLine();
				ImGui::TextColored(ColorDim, "(occupied - clear slots to remove)");
			}

			// Slots table.
			const ImGuiTableFlags TableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
			if (ImGui::BeginTable("BagSlotsTable", 3, TableFlags))
			{
				ImGui::TableSetupColumn("Slot", ImGuiTableColumnFlags_WidthFixed, 40.f);
				ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableHeadersRow();

				for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
				{
					const FArcItemId SlotItem = Bag.Slots[SlotIndex];
					const bool bSlotOccupied = SlotItem.IsValid();

					ImGui::TableNextRow();
					ImGui::PushID(SlotIndex);

					ImGui::TableSetColumnIndex(0);
					ImGui::Text("%d", SlotIndex);

					ImGui::TableSetColumnIndex(1);
					if (bSlotOccupied)
					{
						const FArcItemData* Item = FindItemInStore(Store, SlotItem);
						if (Item != nullptr)
						{
							ImGui::Text("%s  (%s)",
								TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatItemDefinition(Item->GetItemDefinitionId())),
								TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatItemId(SlotItem)));
						}
						else
						{
							ImGui::TextColored(ColorWarn, "%s (not in store)",
								TCHAR_TO_ANSI(*ArcMassItemsDebugger::FormatItemId(SlotItem)));
						}
					}
					else
					{
						ImGui::TextDisabled("(empty)");
					}

					ImGui::TableSetColumnIndex(2);

					// Clear the slot reference (item stays in store).
					ImGui::BeginDisabled(bSlotOccupied == false);
					if (ImGui::SmallButton("Clear") && World != nullptr)
					{
						Arcx::SendServerCommand<FArcMassClearBagSlotCommand>(World, Entity, BagId, SlotIndex);
					}
					ImGui::EndDisabled();

					// Two-click move: first click on an occupied slot arms the source; second
					// click on any slot completes the move to (BagId, SlotIndex).
					ImGui::SameLine();
					const bool bIsPendingSource = PendingBagMove.IsSet()
						&& PendingBagMove.GetValue().Key == BagId
						&& PendingBagMove.GetValue().Value == SlotIndex;
					if (PendingBagMove.IsSet() == false)
					{
						ImGui::BeginDisabled(bSlotOccupied == false);
						if (ImGui::SmallButton("Move"))
						{
							PendingBagMove = TPair<FArcBagId, int32>(BagId, SlotIndex);
						}
						ImGui::EndDisabled();
					}
					else if (bIsPendingSource)
					{
						if (ImGui::SmallButton("Cancel Move"))
						{
							PendingBagMove.Reset();
						}
					}
					else
					{
						if (ImGui::SmallButton("Move Here") && World != nullptr)
						{
							Arcx::SendServerCommand<FArcMassMoveBagItemCommand>(World, Entity,
								PendingBagMove.GetValue().Key, PendingBagMove.GetValue().Value, BagId, SlotIndex);
							PendingBagMove.Reset();
						}
					}

					// Place a resident store item (not already in any bag) into an empty slot.
					if (bSlotOccupied == false && Store != nullptr && Store->ReplicatedItems.Items.Num() > 0)
					{
						ImGui::SameLine();
						if (ImGui::BeginCombo("##PlaceFromStore", "Place from store..."))
						{
							for (const FArcMassReplicatedItem& Replicated : Store->ReplicatedItems.Items)
							{
								const FArcItemData* Item = Replicated.ToItem();
								if (Item == nullptr)
								{
									continue;
								}
								const FArcItemId ItemId = Item->GetItemId();
								if (ArcMassItems::Bags::IsItemInAnyBag(Manager, Entity, ItemId))
								{
									continue;
								}
								const FString Label = FString::Printf(TEXT("%s  (%s)"),
									*ArcMassItemsDebugger::FormatItemDefinition(Item->GetItemDefinitionId()),
									*ArcMassItemsDebugger::FormatItemId(ItemId));
								if (ImGui::Selectable(TCHAR_TO_ANSI(*Label)) && World != nullptr)
								{
									Arcx::SendServerCommand<FArcMassPlaceItemInBagSlotCommand>(World, Entity, BagId, SlotIndex, ItemId);
								}
							}
							ImGui::EndCombo();
						}
					}

					// Create a fresh item directly into this slot, using the shared add controls below.
					if (bSlotOccupied == false)
					{
						ImGui::SameLine();
						const bool bAddDefValid = CachedItemDefinitions.IsValidIndex(BagAddItemSelectedDefIndex);
						ImGui::BeginDisabled(bAddDefValid == false);
						if (ImGui::SmallButton("Add new here") && bAddDefValid)
						{
							const FPrimaryAssetId& AddDefId = CachedItemDefinitions[BagAddItemSelectedDefIndex];
							if (World != nullptr)
							{
								const uint8 LevelArg = static_cast<uint8>(FMath::Clamp(BagAddItemLevel, 1, 255));
								const int32 AmountArg = FMath::Max(BagAddItemAmount, 1);
								UE_LOGF(LogArcBagsDebugger, Log,"AddNewHere clicked: Def=%ls Level=%d Amount=%d Bag=%ls Slot=%d",
									*AddDefId.ToString(), static_cast<int32>(LevelArg), AmountArg,
									*BagId.ToString(), SlotIndex);
								Arcx::SendServerCommand<FArcMassAddNewItemToBagCommand>(World, Entity,
									AddDefId, LevelArg, AmountArg, BagId, SlotIndex);
							}
						}
						ImGui::EndDisabled();
					}

					ImGui::PopID();
				}

				ImGui::EndTable();
			}
		}

		ImGui::PopID();
	}

	ImGui::Separator();

	// --- Add item to first free slot: shared item-definition picker + Level/Amount ---
	ImGui::SeparatorText("Add item to first free slot");
	if (CachedItemDefinitions.Num() == 0)
	{
		RefreshItemDefinitions();
	}

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Filter");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(220.f);
	ImGui::InputText("##BagAddItemFilter", BagItemDefinitionFilterBuf, IM_ARRAYSIZE(BagItemDefinitionFilterBuf));

	const FString Filter = FString(UTF8_TO_TCHAR(BagItemDefinitionFilterBuf)).ToLower();
	if (ImGui::BeginListBox("##BagAddItemDefinitionList", ImVec2(-FLT_MIN, 130.f)))
	{
		for (int32 Index = 0; Index < CachedItemDefinitionNames.Num(); ++Index)
		{
			if (Filter.IsEmpty() == false && CachedItemDefinitionNames[Index].ToLower().Contains(Filter) == false)
			{
				continue;
			}
			if (ImGui::Selectable(TCHAR_TO_ANSI(*CachedItemDefinitionNames[Index]), BagAddItemSelectedDefIndex == Index))
			{
				BagAddItemSelectedDefIndex = Index;
			}
		}
		ImGui::EndListBox();
	}

	// Labels LEFT of the inputs — default ImGui right-side labels read as belonging to
	// the neighbouring field and caused Level/Amount mix-ups.
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Level");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120.f);
	ImGui::InputInt("##BagAddItemLevel", &BagAddItemLevel);
	BagAddItemLevel = FMath::Clamp(BagAddItemLevel, 1, 255);
	ImGui::SameLine();
	ImGui::TextUnformatted("Amount");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120.f);
	ImGui::InputInt("##BagAddItemAmount", &BagAddItemAmount);
	BagAddItemAmount = FMath::Max(BagAddItemAmount, 1);

	const bool bAddDefValid = CachedItemDefinitions.IsValidIndex(BagAddItemSelectedDefIndex);
	ImGui::BeginDisabled(bAddDefValid == false);
	if (ImGui::Button("Add to first free slot") && bAddDefValid)
	{
		const FPrimaryAssetId& AddDefId = CachedItemDefinitions[BagAddItemSelectedDefIndex];
		if (World != nullptr)
		{
			const uint8 LevelArg = static_cast<uint8>(BagAddItemLevel);
			const int32 AmountArg = BagAddItemAmount;
			UE_LOGF(LogArcBagsDebugger, Log,"AddToFirstFreeSlot clicked: Def=%ls Level=%d Amount=%d (raw member BagAddItemAmount=%d)",
				*AddDefId.ToString(), static_cast<int32>(LevelArg), AmountArg, BagAddItemAmount);
			// Default-constructed (invalid) FArcBagId → first free accepting slot across all bags.
			Arcx::SendServerCommand<FArcMassAddNewItemToBagCommand>(World, Entity,
				AddDefId, LevelArg, AmountArg, FArcBagId(), INDEX_NONE);
		}
	}
	ImGui::EndDisabled();
#endif
}
