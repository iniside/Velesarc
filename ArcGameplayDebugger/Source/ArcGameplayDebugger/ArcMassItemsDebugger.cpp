// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassItemsDebugger.h"

#include "ArcMassItemsDebuggerShared.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Attachments/ArcMassItemAttachmentFragments.h"
#include "Equipment/ArcMassEquipmentFragment.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "QuickBar/ArcMassQuickBarStateFragment.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinitionData.h"
#include "Core/ArcCoreAssetManager.h"
#include "JsonAssets/ArcJsonAssetType.h"
#include "GameFramework/Actor.h"
#include "MassActorSubsystem.h"
#include "Mass/Debugger.h"
#include "MassDebuggerWorld.h"
#include "MassEntitySubsystem.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "Mass/EntityView.h"
#include "imgui.h"

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
	CachedBagDefinitions.Reset();
	CachedBagDefinitionNames.Reset();
	SelectedEntityIndex = INDEX_NONE;
	SelectedItemIndex = INDEX_NONE;
	SelectedDefinitionIndex = INDEX_NONE;
	SelectedBagDefinitionIndex = INDEX_NONE;
	PendingBagMove.Reset();
	AddItemMode = EArcMassAddItemMode::None;
	ItemSpecCreator.Uninitialize();
}

void FArcMassItemsDebugger::RefreshItemDefinitions()
{
	CachedItemDefinitions.Reset();
	CachedItemDefinitionNames.Reset();
	SelectedDefinitionIndex = INDEX_NONE;

	const UArcCoreAssetManager& AssetManager = UArcCoreAssetManager::Get();
	for (const FPrimaryAssetType& Type : AssetManager.GetJsonRecordTypes())
	{
		const FArcJsonAssetTypeInfo* TypeInfo = AssetManager.FindJsonRecordType(Type);
		if (TypeInfo == nullptr || TypeInfo->Struct != FArcItemDefinitionData::StaticStruct())
		{
			continue;
		}
		for (const FArcJsonRecordEntry& Entry : AssetManager.GetJsonRecordEntries(Type))
		{
			CachedItemDefinitions.Add(Entry.Id);
			CachedItemDefinitionNames.Add(Entry.DisplayName);
		}
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

	UE::Mass::FSparseElementIterator It = Manager->CreateSparseElementsIterator(
		FArcMassItemStoreFragment::StaticStruct());
	for (; It; ++It)
	{
		const FMassEntityHandle Entity = It.GetEntityHandle(*Manager);
		const FStructView StoreView = It.GetElementView();
		const FArcMassItemStoreFragment* Store = StoreView.GetPtr<const FArcMassItemStoreFragment>();
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

		FMassEntityView View(*Manager, Entity);
		Entry.bHasQuickBar = View.GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>() != nullptr;
		const FArcMassItemAttachmentStateFragment* AttachState =
			View.GetSparseFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
		Entry.AttachmentCount = AttachState ? AttachState->ActiveAttachments.Num() : 0;
		const FArcMassEquipmentFragment* EquipFrag =
			View.GetSparseFragmentDataPtr<FArcMassEquipmentFragment>();
		Entry.EquippedCount = EquipFrag ? EquipFrag->EquippedItems.Num() : 0;

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

	if (SelectedEntityIndex >= CachedEntities.Num())
	{
		SelectedEntityIndex = INDEX_NONE;
		SelectedItemIndex = INDEX_NONE;
	}
#endif
}

void FArcMassItemsDebugger::RequestJumpToItem(FArcItemId ItemId)
{
	PendingJumpItemId = ItemId;
	PendingJump = EArcMassItemsDebuggerJumpTarget::ItemsTab;
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
	ImGui::SameLine();
	ImGui::Checkbox("Pause auto-refresh", &bPauseAutoRefresh);

	UWorld* World = ArcMassItemsDebugger::GetDebugWorld();
	if (World && !bPauseAutoRefresh)
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
			const FString DisplayLabel = FString::Printf(
				TEXT("%s  [I:%d S:%d Q:%s A:%d E:%d]"),
				*Entry.Label, Entry.ItemCount, Entry.SlottedItemCount,
				Entry.bHasQuickBar ? TEXT("Y") : TEXT("-"),
				Entry.AttachmentCount, Entry.EquippedCount);
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

	ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s",
		TCHAR_TO_ANSI(*CachedEntities[SelectedEntityIndex].Label));
	ImGui::SameLine();
	ImGui::TextDisabled("Entity: %d:%d", Entity.Index, Entity.SerialNumber);
	ImGui::Separator();

	if (PendingJump == EArcMassItemsDebuggerJumpTarget::ItemsTab)
	{
		ActiveTab = EActiveTab::Items;
	}

	if (ImGui::BeginTabBar("MassItemsTopTabs"))
	{
		const ImGuiTabItemFlags ItemsFlags =
			(PendingJump == EArcMassItemsDebuggerJumpTarget::ItemsTab) ? ImGuiTabItemFlags_SetSelected : 0;
		if (ImGui::BeginTabItem("Items", nullptr, ItemsFlags))
		{
			ActiveTab = EActiveTab::Items;
			DrawItemsTab(Manager, Entity);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("QuickBars"))
		{
			ActiveTab = EActiveTab::QuickBars;
			DrawQuickBarTab(Manager, Entity);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Attachments"))
		{
			ActiveTab = EActiveTab::Attachments;
			DrawAttachmentsTab(Manager, Entity);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Equipment"))
		{
			ActiveTab = EActiveTab::Equipment;
			DrawEquipmentTab(Manager, Entity);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Bags"))
		{
			ActiveTab = EActiveTab::Bags;
			DrawBagsTab(Manager, Entity);
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	PendingJump = EArcMassItemsDebuggerJumpTarget::None;
#endif
}
