// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassItemsDebugger.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Fragments/ArcMassItemEventFragment.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemInstance.h"
#include "Items/ArcItemSpec.h"
#include "MassActorSubsystem.h"
#include "MassDebugger.h"
#include "MassEntitySubsystem.h"
#include "MassEntityQuery.h"
#include "MassEntityView.h"
#include "MassExecutionContext.h"
#include "Operations/ArcMassItemOperations.h"
#include "StructUtils/InstancedStruct.h"
#include "Traits/ArcMassItemStoreTrait.h"
#include "imgui.h"

namespace ArcMassItemsDebugger
{
	UWorld* GetDebugWorld()
	{
		if (!GEngine || !GEngine->GameViewport)
		{
			return nullptr;
		}
		return GEngine->GameViewport->GetWorld();
	}

	FMassEntityManager* GetEntityManager()
	{
		UWorld* World = GetDebugWorld();
		if (!World)
		{
			return nullptr;
		}

		UMassEntitySubsystem* MassSub = World->GetSubsystem<UMassEntitySubsystem>();
		if (!MassSub)
		{
			return nullptr;
		}

		return &MassSub->GetMutableEntityManager();
	}
} // namespace ArcMassItemsDebugger

namespace
{
	FArcMassItemStoreFragment* GetStore(FMassEntityManager& Manager, FMassEntityHandle Entity)
	{
		FMassEntityView EntityView(Manager, Entity);
		return EntityView.GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
	}

	FString FormatGameplayTag(const FGameplayTag& Tag)
	{
		return Tag.IsValid() ? Tag.ToString() : TEXT("-");
	}

	FString FormatItemId(const FArcItemId& ItemId)
	{
		return ItemId.IsValid() ? ItemId.ToString() : TEXT("-");
	}

	const FArcItemData* GetItemAt(const FArcMassItemStoreFragment& Store, int32 Index)
	{
		if (!Store.ReplicatedItems.Items.IsValidIndex(Index))
		{
			return nullptr;
		}
		return Store.ReplicatedItems.Items[Index].ToItem();
	}

	const char* ToEventTypeName(EArcMassItemEventType Type)
	{
		switch (Type)
		{
		case EArcMassItemEventType::Added: return "Added";
		case EArcMassItemEventType::Removed: return "Removed";
		case EArcMassItemEventType::SlotChanged: return "SlotChanged";
		case EArcMassItemEventType::StacksChanged: return "StacksChanged";
		case EArcMassItemEventType::Attached: return "Attached";
		case EArcMassItemEventType::Detached: return "Detached";
		default: return "Unknown";
		}
	}
}

void FArcMassItemsDebugger::Initialize()
{
	SelectedEntityIndex = INDEX_NONE;
	SelectedItemIndex = INDEX_NONE;
	EntityFilterBuf[0] = '\0';
	ItemDefinitionFilterBuf[0] = '\0';
	SlotTagBuf[0] = '\0';
	LastRefreshTime = 0.f;
	AddItemMode = EArcMassAddItemMode::None;
	ItemSpecCreator.Initialize();
	RefreshItemDefinitions();
	RefreshEntityList();
}

void FArcMassItemsDebugger::Uninitialize()
{
	CachedEntities.Reset();
	CachedItemDefinitions.Reset();
	CachedItemDefinitionNames.Reset();
	SelectedEntityIndex = INDEX_NONE;
	SelectedItemIndex = INDEX_NONE;
	SelectedDefinitionIndex = INDEX_NONE;
	AddItemMode = EArcMassAddItemMode::None;
	ItemSpecCreator.Uninitialize();
}

void FArcMassItemsDebugger::RefreshItemDefinitions()
{
	CachedItemDefinitions.Reset();
	CachedItemDefinitionNames.Reset();
	SelectedDefinitionIndex = INDEX_NONE;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	AssetRegistry.GetAssetsByClass(UArcItemDefinition::StaticClass()->GetClassPathName(), CachedItemDefinitions, true);

	for (const FAssetData& AssetData : CachedItemDefinitions)
	{
		CachedItemDefinitionNames.Add(AssetData.AssetName.ToString());
	}

	if (CachedItemDefinitions.Num() > 0)
	{
		SelectedDefinitionIndex = 0;
	}
}

void FArcMassItemsDebugger::RefreshEntityList()
{
#if WITH_MASSENTITY_DEBUG
	CachedEntities.Reset();

	FMassEntityManager* Manager = ArcMassItemsDebugger::GetEntityManager();
	if (!Manager)
	{
		return;
	}

	FMassEntityQuery Query(Manager->AsShared());
	Query.AddTagRequirement<FArcMassItemStoreTag>(EMassFragmentPresence::All);

	FMassExecutionContext ExecutionContext = Manager->CreateExecutionContext(0.f);
	Query.ForEachEntityChunk(ExecutionContext, [this, Manager](FMassExecutionContext& Context)
	{
		const TConstArrayView<FMassEntityHandle> Entities = Context.GetEntities();
		for (int32 EntityIndex = 0; EntityIndex < Entities.Num(); ++EntityIndex)
		{
			const FMassEntityHandle Entity = Entities[EntityIndex];
			const FArcMassItemStoreFragment* Store = GetStore(*Manager, Entity);
			if (!Store)
			{
				continue;
			}

			FEntityEntry& Entry = CachedEntities.AddDefaulted_GetRef();
			Entry.Entity = Entity;
			Entry.ItemCount = Store->ReplicatedItems.Items.Num();
			{
				int32 SlottedCount = 0;
				for (const FArcMassReplicatedItem& ReplicatedItem : Store->ReplicatedItems.Items)
				{
					const FArcItemData* ItemData = ReplicatedItem.ToItem();
					if (ItemData && ItemData->GetSlotId().IsValid())
					{
						++SlottedCount;
					}
				}
				Entry.SlottedItemCount = SlottedCount;
			}

			FStructView ActorFragView = Manager->GetFragmentDataStruct(Entity, FMassActorFragment::StaticStruct());
			if (ActorFragView.IsValid())
			{
				const FMassActorFragment& ActorFrag = ActorFragView.Get<FMassActorFragment>();
				if (const AActor* Actor = ActorFrag.Get())
				{
					Entry.Label = Actor->GetActorNameOrLabel();
				}
			}

			if (Entry.Label.IsEmpty())
			{
				Entry.Label = FString::Printf(TEXT("E%d"), Entity.Index);
			}
		}
	});

	if (SelectedEntityIndex >= CachedEntities.Num())
	{
		SelectedEntityIndex = INDEX_NONE;
		SelectedItemIndex = INDEX_NONE;
	}
#endif
}

void FArcMassItemsDebugger::Draw()
{
#if WITH_MASSENTITY_DEBUG
	ImGui::SetNextWindowSize(ImVec2(1250.f, 700.f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Mass Items Debugger", &bShow))
	{
		ImGui::End();
		return;
	}

	FMassEntityManager* Manager = ArcMassItemsDebugger::GetEntityManager();
	if (!Manager)
	{
		ImGui::TextDisabled("No MassEntitySubsystem available");
		ImGui::End();
		return;
	}

	if (ImGui::Button("Refresh"))
	{
		RefreshEntityList();
	}
	ImGui::SameLine();
	ImGui::Text("Entities with item stores: %d", CachedEntities.Num());

	UWorld* World = ArcMassItemsDebugger::GetDebugWorld();
	if (World)
	{
		const float CurrentTime = World->GetTimeSeconds();
		if (CurrentTime - LastRefreshTime > 2.0f)
		{
			LastRefreshTime = CurrentTime;
			RefreshEntityList();
		}
	}

	ImGui::Separator();

	const float LeftPanelWidth = ImGui::GetContentRegionAvail().x * 0.25f;
	if (ImGui::BeginChild("MassItemsEntityListPanel", ImVec2(LeftPanelWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
	{
		DrawEntityListPanel();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	if (ImGui::BeginChild("MassItemsDetailPanel", ImVec2(0, 0), ImGuiChildFlags_Borders))
	{
		if (SelectedEntityIndex == INDEX_NONE || !CachedEntities.IsValidIndex(SelectedEntityIndex))
		{
			ImGui::TextDisabled("Select an entity from the list");
		}
		else
		{
			DrawDetailPanel(*Manager, CachedEntities[SelectedEntityIndex].Entity);
		}
	}
	ImGui::EndChild();

	ImGui::End();
#endif
}

void FArcMassItemsDebugger::DrawEntityListPanel()
{
#if WITH_MASSENTITY_DEBUG
	ImGui::Text("Entities");
	ImGui::Separator();
	ImGui::InputText("Filter", EntityFilterBuf, IM_ARRAYSIZE(EntityFilterBuf));

	const FString Filter = FString(UTF8_TO_TCHAR(EntityFilterBuf)).ToLower();
	if (ImGui::BeginChild("MassItemsEntityScroll", ImVec2(0, 0)))
	{
		for (int32 Index = 0; Index < CachedEntities.Num(); ++Index)
		{
			const FEntityEntry& Entry = CachedEntities[Index];
			const FString DisplayLabel = FString::Printf(TEXT("%s  [%d items, %d slots]"), *Entry.Label, Entry.ItemCount, Entry.SlottedItemCount);
			if (!Filter.IsEmpty() && !DisplayLabel.ToLower().Contains(Filter))
			{
				continue;
			}

			if (ImGui::Selectable(TCHAR_TO_ANSI(*DisplayLabel), SelectedEntityIndex == Index))
			{
				SelectedEntityIndex = Index;
				SelectedItemIndex = INDEX_NONE;
				AddItemMode = EArcMassAddItemMode::None;
				ItemSpecCreator.Reset();
			}
		}
	}
	ImGui::EndChild();
#endif
}

void FArcMassItemsDebugger::DrawDetailPanel(FMassEntityManager& Manager, FMassEntityHandle Entity)
{
#if WITH_MASSENTITY_DEBUG
	if (!Manager.IsEntityActive(Entity))
	{
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Entity is no longer active");
		return;
	}

	FArcMassItemStoreFragment* Store = GetStore(Manager, Entity);
	if (!Store)
	{
		ImGui::TextDisabled("Selected entity no longer has FArcMassItemStoreFragment");
		return;
	}

	ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", TCHAR_TO_ANSI(*CachedEntities[SelectedEntityIndex].Label));
	ImGui::SameLine();
	ImGui::TextDisabled("Entity: %d:%d", Entity.Index, Entity.SerialNumber);
	ImGui::Separator();

	DrawAddItemPanel(Manager, Entity);
	ImGui::Spacing();

	const float TopHeight = ImGui::GetContentRegionAvail().y * 0.42f;
	if (ImGui::BeginChild("MassItemsStoreTop", ImVec2(0, TopHeight), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY))
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
				ArcMassItems::AddItem(Manager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemSpecCreator.GetResultSpec());
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
			UArcItemDefinition* Definition = Cast<UArcItemDefinition>(CachedItemDefinitions[SelectedDefinitionIndex].GetAsset());
			if (Definition)
			{
				ArcMassItems::AddItem(Manager, Entity, FArcMassItemStoreFragment::StaticStruct(), FArcItemSpec::NewItem(Definition, 1, 1));
				RefreshEntityList();
			}
		}
	}
	else if (AddItemMode == EArcMassAddItemMode::CustomSpec)
	{
		if (ImGui::Button("Edit Spec...") && CachedItemDefinitions.IsValidIndex(SelectedDefinitionIndex))
		{
			UArcItemDefinition* Definition = Cast<UArcItemDefinition>(CachedItemDefinitions[SelectedDefinitionIndex].GetAsset());
			if (Definition)
			{
				ItemSpecCreator.Begin(Definition);
			}
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
			if (ImGui::Selectable(TCHAR_TO_ANSI(*GetNameSafe(Item->GetItemDefinition())), SelectedItemIndex == Index, ImGuiSelectableFlags_SpanAllColumns))
			{
				SelectedItemIndex = Index;
			}

			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%s", TCHAR_TO_ANSI(*FormatItemId(Item->GetItemId())));

			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%d", Item->GetStacks());

			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%d", Item->GetLevel());

			ImGui::TableSetColumnIndex(4);
			ImGui::Text("%s", TCHAR_TO_ANSI(*FormatGameplayTag(Item->GetSlotId())));

			ImGui::TableSetColumnIndex(5);
			ImGui::Text("%u", Item->GetVersion());

			ImGui::TableSetColumnIndex(6);
			if (ImGui::SmallButton("Remove"))
			{
				ArcMassItems::RemoveItem(Manager, Entity, FArcMassItemStoreFragment::StaticStruct(), Item->GetItemId());
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

	const FArcItemData* Item = GetItemAt(Store, SelectedItemIndex);
	if (!Item)
	{
		ImGui::TextDisabled("Selected item has invalid item data");
		return;
	}

	DrawItemDetail(Manager, Entity, *Item);
}

void FArcMassItemsDebugger::DrawItemDetail(FMassEntityManager& Manager, FMassEntityHandle Entity, const FArcItemData& Item)
{
	const UArcItemDefinition* Definition = Item.GetItemDefinition();

	if (ImGui::BeginTabBar("MassItemDetailTabs"))
	{
		if (ImGui::BeginTabItem("Overview"))
		{
			ImGui::Text("Definition: %s", TCHAR_TO_ANSI(*GetNameSafe(Definition)));
			ImGui::Text("DefinitionId: %s", TCHAR_TO_ANSI(*Item.GetItemDefinitionId().ToString()));
			ImGui::Text("ItemId: %s", TCHAR_TO_ANSI(*FormatItemId(Item.GetItemId())));
			ImGui::Text("Level: %d", Item.GetLevel());
			ImGui::Text("Stacks: %d", Item.GetStacks());
			ImGui::Text("Version: %u", Item.GetVersion());
			ImGui::Text("Slot: %s", TCHAR_TO_ANSI(*FormatGameplayTag(Item.GetSlotId())));
			ImGui::Text("Attach Slot: %s", TCHAR_TO_ANSI(*FormatGameplayTag(Item.GetAttachSlot())));
			ImGui::Text("OwnerId: %s", TCHAR_TO_ANSI(*FormatItemId(Item.GetOwnerId())));
			ImGui::Text("OldOwnerId: %s", TCHAR_TO_ANSI(*FormatItemId(Item.OldOwnerId)));
			ImGui::Text("OldSlot: %s", TCHAR_TO_ANSI(*FormatGameplayTag(Item.OldSlot)));
			ImGui::Text("OldAttachedToSlot: %s", TCHAR_TO_ANSI(*FormatGameplayTag(Item.OldAttachedToSlot)));
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
						ArcMassItems::AddItemToSlot(Manager, Entity, FArcMassItemStoreFragment::StaticStruct(), Item.GetItemId(), SlotTag);
					}
				}
			}
			if (Item.GetSlotId().IsValid())
			{
				ImGui::SameLine();
				if (ImGui::Button("Remove Slot"))
				{
					ArcMassItems::RemoveItemFromSlot(Manager, Entity, FArcMassItemStoreFragment::StaticStruct(), Item.GetItemId());
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

			DrawTags("Dynamic Tags", Item.DynamicTags);
			DrawTags("Aggregated Tags", Item.GetItemAggregatedTags());

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Spec"))
		{
			DrawStructProperties(FArcItemSpec::StaticStruct(), &Item.Spec, "MassItemSpec");
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
					ImGui::Text("%s", TCHAR_TO_ANSI(*FormatItemId(Event.ItemId)));
					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%s", TCHAR_TO_ANSI(*FormatGameplayTag(Event.SlotTag)));
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

void FArcMassItemsDebugger::DrawStructProperties(const UScriptStruct* StructType, const void* StructData, const char* TableIdPrefix) const
{
	if (!StructType || !StructData)
	{
		ImGui::TextDisabled("(invalid struct)");
		return;
	}

	const FString TableId = FString::Printf(TEXT("%s_%s"), ANSI_TO_TCHAR(TableIdPrefix), *StructType->GetName());
	if (ImGui::BeginTable(TCHAR_TO_ANSI(*TableId), 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
	{
		ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 220.f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();

		for (TFieldIterator<FProperty> PropIt(StructType); PropIt; ++PropIt)
		{
			const FProperty* Property = *PropIt;
			FString Value;
			const void* PropertyData = Property->ContainerPtrToValuePtr<void>(StructData);
			Property->ExportTextItem_Direct(Value, PropertyData, nullptr, nullptr, PPF_None);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%s", TCHAR_TO_ANSI(*Property->GetName()));
			ImGui::TableSetColumnIndex(1);
			ImGui::TextWrapped("%s", TCHAR_TO_ANSI(*Value));
		}

		ImGui::EndTable();
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
				DrawStructProperties(StructType, Instance, "RuntimeInstanceData");
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
				DrawStructProperties(StructType, InstanceStruct.GetMemory(), "ReplicatedItemInstance");
				ImGui::TreePop();
			}
		}
	}
}

void FArcMassItemsDebugger::DrawDefinitionFragments(const UArcItemDefinition* Definition) const
{
	if (!Definition)
	{
		ImGui::TextDisabled("No item definition loaded");
		return;
	}

	ImGui::Text("Path: %s", TCHAR_TO_ANSI(*Definition->GetPathName()));
	ImGui::Text("PrimaryAssetId: %s", TCHAR_TO_ANSI(*Definition->GetPrimaryAssetId().ToString()));

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
			DrawStructProperties(Fragment.GetScriptStruct(), Fragment.GetMemory(), "DefinitionFragment");
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
				DrawStructProperties(Fragment.GetScriptStruct(), Fragment.GetMemory(), "DefinitionScalableFragment");
				ImGui::TreePop();
			}
		}
	}
}

void FArcMassItemsDebugger::DrawTags(const char* Header, const FGameplayTagContainer& Tags) const
{
	if (Tags.Num() == 0)
	{
		return;
	}

	ImGui::Spacing();
	ImGui::SeparatorText(Header);
	TArray<FGameplayTag> TagArray;
	Tags.GetGameplayTagArray(TagArray);
	for (const FGameplayTag& Tag : TagArray)
	{
		ImGui::BulletText("%s", TCHAR_TO_ANSI(*Tag.ToString()));
	}
}
