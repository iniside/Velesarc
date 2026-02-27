// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcItemDebuggerItems.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Commands/ArcAddItemToQuickBarCommand.h"
#include "Commands/ArcDropItemCommand.h"
#include "Commands/ArcMoveItemBetweenStoresCommand.h"
#include "Commands/ArcReplicatedCommandHelpers.h"
#include "Engine/Engine.h"
#include "UObject/UObjectIterator.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/ArcItemsStoreComponent.h"
#include "Items/Fragments/ArcItemFragment_Droppable.h"
#include "QuickBar/ArcQuickBarComponent.h"
#include "imgui.h"

void FArcDebuggerItems::Initialize()
{
	RefreshItemDefinitions();
}

void FArcDebuggerItems::Uninitialize()
{
	CachedItemDefinitions.Reset();
	CachedItemDefinitionNames.Reset();
	SelectedStore.Reset();
	TargetStore.Reset();
	TargetStoreIndex = -1;
	SelectedQuickBar.Reset();
}

TArray<UArcItemsStoreComponent*> FArcDebuggerItems::GetPlayerStores() const
{
	TArray<UArcItemsStoreComponent*> Result;

	UWorld* World = GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
	if (!World)
	{
		return Result;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return Result;
	}

	APawn* Pawn = PC->GetPawn();

	TArray<AActor*> ActorsToSearch;
	if (Pawn)
	{
		ActorsToSearch.Add(Pawn);
	}
	ActorsToSearch.Add(PC);
	if (PC->GetPlayerState<APlayerState>())
	{
		ActorsToSearch.Add(PC->GetPlayerState<APlayerState>());
	}

	for (AActor* Actor : ActorsToSearch)
	{
		if (!Actor)
		{
			continue;
		}
		TArray<UArcItemsStoreComponent*> Components;
		Actor->GetComponents<UArcItemsStoreComponent>(Components);
		for (UArcItemsStoreComponent* Comp : Components)
		{
			Result.AddUnique(Comp);
		}
	}

	return Result;
}

TArray<UArcItemsStoreComponent*> FArcDebuggerItems::GetAllWorldStores() const
{
	TArray<UArcItemsStoreComponent*> Result;

	UWorld* World = GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
	if (!World)
	{
		return Result;
	}

	for (TObjectIterator<UArcItemsStoreComponent> It; It; ++It)
	{
		UArcItemsStoreComponent* Comp = *It;
		if (Comp && Comp->GetWorld() == World)
		{
			Result.Add(Comp);
		}
	}

	return Result;
}

TArray<UArcQuickBarComponent*> FArcDebuggerItems::GetPlayerQuickBars() const
{
	TArray<UArcQuickBarComponent*> Result;

	UWorld* World = GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
	if (!World)
	{
		return Result;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return Result;
	}

	APawn* Pawn = PC->GetPawn();

	TArray<AActor*> ActorsToSearch;
	if (Pawn)
	{
		ActorsToSearch.Add(Pawn);
	}
	ActorsToSearch.Add(PC);
	if (PC->GetPlayerState<APlayerState>())
	{
		ActorsToSearch.Add(PC->GetPlayerState<APlayerState>());
	}

	for (AActor* Actor : ActorsToSearch)
	{
		if (!Actor)
		{
			continue;
		}
		TArray<UArcQuickBarComponent*> Components;
		Actor->GetComponents<UArcQuickBarComponent>(Components);
		for (UArcQuickBarComponent* Comp : Components)
		{
			Result.AddUnique(Comp);
		}
	}

	return Result;
}

void FArcDebuggerItems::RefreshItemDefinitions()
{
	CachedItemDefinitions.Reset();
	CachedItemDefinitionNames.Reset();

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	AssetRegistry.GetAssetsByClass(UArcItemDefinition::StaticClass()->GetClassPathName(), CachedItemDefinitions, true);

	for (const FAssetData& AssetData : CachedItemDefinitions)
	{
		CachedItemDefinitionNames.Add(AssetData.AssetName.ToString());
	}
}

void FArcDebuggerItems::Draw()
{
	if (!GEngine || !GEngine->GameViewport)
	{
		return;
	}

	ImGui::Begin("Items Debug", &bShow);

	if (ImGui::BeginTabBar("ArcItemsDebugTabs"))
	{
		if (ImGui::BeginTabItem("Items"))
		{
			DrawItemsTab();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("QuickBar"))
		{
			DrawQuickBarTab();
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::End();
}

void FArcDebuggerItems::DrawItemsTab()
{
	DrawStoreSelector();

	if (SelectedStore.IsValid())
	{
		ImGui::Separator();

		if (ImGui::BeginTable("ItemsLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
		{
			ImGui::TableNextRow();

			// Left panel: items list
			ImGui::TableSetColumnIndex(0);
			DrawItemsList();

			// Right panel: item detail
			ImGui::TableSetColumnIndex(1);
			TArray<const FArcItemData*> Items = SelectedStore->GetItems();
			if (SelectedItemIndex >= 0 && SelectedItemIndex < Items.Num())
			{
				DrawItemDetail(Items[SelectedItemIndex]);
			}
			else
			{
				ImGui::TextDisabled("Select an item to view details");
			}

			ImGui::EndTable();
		}
	}
	else
	{
		ImGui::TextDisabled("No ItemsStoreComponent selected");
	}
}

static FString FormatStoreName(UArcItemsStoreComponent* Store)
{
	return FString::Printf(TEXT("%s (%s) [%d items]"),
		*Store->GetName(),
		*GetNameSafe(Store->GetOwner()),
		Store->GetItemNum());
}

void FArcDebuggerItems::DrawStoreSelector()
{
	TArray<UArcItemsStoreComponent*> Stores = GetAllWorldStores();

	ImGui::Text("Source Store:");
	ImGui::SameLine();

	if (Stores.Num() == 0)
	{
		ImGui::TextDisabled("No stores found in world");
		return;
	}

	// Auto-select first store if none selected
	if (!SelectedStore.IsValid() && Stores.Num() > 0)
	{
		SelectedStoreIndex = 0;
		SelectedStore = Stores[0];
	}

	// Validate index against current array
	if (SelectedStore.IsValid())
	{
		int32 FoundIdx = Stores.IndexOfByKey(SelectedStore.Get());
		if (FoundIdx != INDEX_NONE)
		{
			SelectedStoreIndex = FoundIdx;
		}
	}

	FString PreviewStr = TEXT("Select Store");
	if (Stores.IsValidIndex(SelectedStoreIndex) && SelectedStore.IsValid())
	{
		PreviewStr = FormatStoreName(Stores[SelectedStoreIndex]);
	}

	ImGui::SetNextItemWidth(400.f);
	if (ImGui::BeginCombo("##StoreSelector", TCHAR_TO_ANSI(*PreviewStr)))
	{
		for (int32 Idx = 0; Idx < Stores.Num(); Idx++)
		{
			FString Name = FormatStoreName(Stores[Idx]);

			if (ImGui::Selectable(TCHAR_TO_ANSI(*Name), SelectedStoreIndex == Idx))
			{
				SelectedStoreIndex = Idx;
				SelectedStore = Stores[Idx];
				SelectedItemIndex = -1;
			}
		}
		ImGui::EndCombo();
	}

	// Target store selector
	ImGui::Text("Target Store:");
	ImGui::SameLine();

	// Validate target index
	if (TargetStore.IsValid())
	{
		int32 FoundIdx = Stores.IndexOfByKey(TargetStore.Get());
		if (FoundIdx != INDEX_NONE)
		{
			TargetStoreIndex = FoundIdx;
		}
		else
		{
			TargetStore.Reset();
			TargetStoreIndex = -1;
		}
	}

	FString TargetPreviewStr = TEXT("Select Target Store");
	if (Stores.IsValidIndex(TargetStoreIndex) && TargetStore.IsValid())
	{
		TargetPreviewStr = FormatStoreName(Stores[TargetStoreIndex]);
	}

	ImGui::SetNextItemWidth(400.f);
	if (ImGui::BeginCombo("##TargetStoreSelector", TCHAR_TO_ANSI(*TargetPreviewStr)))
	{
		// "None" option to clear target
		if (ImGui::Selectable("(None)", TargetStoreIndex == -1))
		{
			TargetStoreIndex = -1;
			TargetStore.Reset();
		}

		for (int32 Idx = 0; Idx < Stores.Num(); Idx++)
		{
			FString Name = FormatStoreName(Stores[Idx]);

			if (ImGui::Selectable(TCHAR_TO_ANSI(*Name), TargetStoreIndex == Idx))
			{
				TargetStoreIndex = Idx;
				TargetStore = Stores[Idx];
			}
		}
		ImGui::EndCombo();
	}

	if (TargetStore.IsValid() && TargetStore == SelectedStore)
	{
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.f, 0.5f, 0.f, 1.f), "(same as source)");
	}
}

void FArcDebuggerItems::DrawItemsList()
{
	if (!SelectedStore.IsValid())
	{
		return;
	}

	// Add Item button
	if (ImGui::Button("Add Item"))
	{
		bShowAddItemPanel = !bShowAddItemPanel;
		if (bShowAddItemPanel && CachedItemDefinitions.Num() == 0)
		{
			RefreshItemDefinitions();
		}
	}

	if (bShowAddItemPanel)
	{
		DrawAddItemPanel();
	}

	// Items table
	const ImGuiTableFlags TableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
	if (ImGui::BeginTable("ItemsTable", 5, TableFlags, ImVec2(0.f, 0.f)))
	{
		ImGui::TableSetupColumn("Definition");
		ImGui::TableSetupColumn("Stacks");
		ImGui::TableSetupColumn("Level");
		ImGui::TableSetupColumn("Slot");
		ImGui::TableSetupColumn("Actions");
		ImGui::TableHeadersRow();

		TArray<const FArcItemData*> Items = SelectedStore->GetItems();
		for (int32 i = 0; i < Items.Num(); ++i)
		{
			const FArcItemData* Item = Items[i];
			if (!Item)
			{
				continue;
			}

			ImGui::TableNextRow();
			ImGui::PushID(i);

			// Definition column
			ImGui::TableSetColumnIndex(0);
			const UArcItemDefinition* Def = Item->GetItemDefinition();
			bool bSelected = SelectedItemIndex == i;
			if (ImGui::Selectable(TCHAR_TO_ANSI(*GetNameSafe(Def)), bSelected, ImGuiSelectableFlags_SpanAllColumns))
			{
				SelectedItemIndex = i;
			}

			// Stacks column
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%d", Item->GetStacks());

			// Level column
			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%d", Item->GetLevel());

			// Slot column
			ImGui::TableSetColumnIndex(3);
			FString SlotStr = Item->Slot.IsValid() ? Item->Slot.ToString() : TEXT("-");
			ImGui::Text(TCHAR_TO_ANSI(*SlotStr));

			// Actions column
			ImGui::TableSetColumnIndex(4);
			if (ImGui::SmallButton("Remove"))
			{
				SelectedStore->RemoveItem(Item->GetItemId(), Item->GetStacks());
				if (SelectedItemIndex >= Items.Num() - 1)
				{
					SelectedItemIndex = Items.Num() - 2;
				}
			}

			// Drop button - only if item has FArcItemFragment_Droppable
			const UArcItemDefinition* ItemDef = Item->GetItemDefinition();
			if (ItemDef)
			{
				const FArcItemFragment_Droppable* DroppableFrag = ItemDef->FindFragment<FArcItemFragment_Droppable>();
				if (DroppableFrag && DroppableFrag->DropEntityConfig)
				{
					ImGui::SameLine();
					if (ImGui::SmallButton("Drop"))
					{
						UWorld* World = GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
						if (World)
						{
							FTransform DropTransform = FTransform::Identity;
							APlayerController* PC = World->GetFirstPlayerController();
							if (PC && PC->GetPawn())
							{
								DropTransform = PC->GetPawn()->GetActorTransform();
								FVector Forward = DropTransform.GetRotation().GetForwardVector();
								DropTransform.SetLocation(DropTransform.GetLocation() + Forward * 100.f);
							}

							Arcx::SendServerCommand<FArcDropItemCommand>(
								World,
								SelectedStore.Get(),
								Item->GetItemId(),
								DropTransform,
								0);
						}
					}
				}
			}

			// Move button - only if target store is selected and different from source
			if (TargetStore.IsValid() && TargetStore != SelectedStore)
			{
				ImGui::SameLine();
				if (ImGui::SmallButton("Move"))
				{
					UWorld* World = GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
					if (World)
					{
						Arcx::SendServerCommand<FArcMoveItemBetweenStoresCommand>(
							World,
							SelectedStore.Get(),
							TargetStore.Get(),
							Item->GetItemId(),
							Item->GetStacks());

						if (SelectedItemIndex >= Items.Num() - 1)
						{
							SelectedItemIndex = Items.Num() - 2;
						}
					}
				}
			}

			ImGui::PopID();
		}

		ImGui::EndTable();
	}
}

void FArcDebuggerItems::DrawAddItemPanel()
{
	ImGui::Separator();
	ImGui::Text("Add New Item");

	ImGui::SetNextItemWidth(200.f);
	if (ImGui::InputText("Filter##AddItemFilter", FilterBuf, IM_ARRAYSIZE(FilterBuf)))
	{
		SelectedDefinitionIndex = 0;
	}

	ImGui::SameLine();
	if (ImGui::Button("Refresh"))
	{
		RefreshItemDefinitions();
		FMemory::Memzero(FilterBuf, sizeof(FilterBuf));
	}

	FString FilterStr = FString(UTF8_TO_TCHAR(FilterBuf)).ToLower();

	if (ImGui::BeginListBox("##DefList", ImVec2(-FLT_MIN, 150.f)))
	{
		for (int32 i = 0; i < CachedItemDefinitionNames.Num(); ++i)
		{
			if (!FilterStr.IsEmpty() && !CachedItemDefinitionNames[i].ToLower().Contains(FilterStr))
			{
				continue;
			}

			bool bSelected = SelectedDefinitionIndex == i;
			if (ImGui::Selectable(TCHAR_TO_ANSI(*CachedItemDefinitionNames[i]), bSelected))
			{
				SelectedDefinitionIndex = i;
			}
		}
		ImGui::EndListBox();
	}

	if (ImGui::Button("Add Selected"))
	{
		if (SelectedDefinitionIndex >= 0 && SelectedDefinitionIndex < CachedItemDefinitions.Num())
		{
			UArcItemDefinition* Def = Cast<UArcItemDefinition>(CachedItemDefinitions[SelectedDefinitionIndex].GetAsset());
			if (Def && SelectedStore.IsValid())
			{
				FArcItemSpec Spec = FArcItemSpec::NewItem(Def, 1, 1);
				SelectedStore->AddItem(Spec, FArcItemId());
			}
		}
	}

	ImGui::SameLine();
	if (ImGui::Button("Close##AddPanel"))
	{
		bShowAddItemPanel = false;
	}

	ImGui::Separator();
}

void FArcDebuggerItems::DrawItemDetail(const FArcItemData* Item)
{
	if (!Item)
	{
		return;
	}

	if (ImGui::BeginChild("ItemDetail", ImVec2(0.f, 0.f), ImGuiChildFlags_None))
	{
		const UArcItemDefinition* Def = Item->GetItemDefinition();

		// Basic info
		ImGui::Text("Definition: %s", TCHAR_TO_ANSI(*GetNameSafe(Def)));
		ImGui::Text("ItemId: %s", TCHAR_TO_ANSI(*Item->GetItemId().ToString()));
		ImGui::Text("Level: %d", Item->GetLevel());
		ImGui::Text("Stacks: %d", Item->GetStacks());

		FString SlotStr = Item->Slot.IsValid() ? Item->Slot.ToString() : TEXT("None");
		ImGui::Text("Slot: %s", TCHAR_TO_ANSI(*SlotStr));

		if (Item->OwnerId.IsValid())
		{
			ImGui::Text("Owner: %s", TCHAR_TO_ANSI(*Item->OwnerId.ToString()));
		}

		// Droppable status
		{
			bool bDroppable = false;
			if (Def)
			{
				const FArcItemFragment_Droppable* DroppableFrag = Def->FindFragment<FArcItemFragment_Droppable>();
				bDroppable = DroppableFrag && DroppableFrag->DropEntityConfig != nullptr;
			}
			ImGui::Text("Droppable: %s", bDroppable ? "Yes" : "No");
		}

		// Slot controls
		ImGui::Spacing();
		ImGui::SeparatorText("Slot Controls");
		ImGui::SetNextItemWidth(200.f);
		ImGui::InputText("##SlotTag", SlotTagBuf, IM_ARRAYSIZE(SlotTagBuf));
		ImGui::SameLine();
		if (ImGui::Button("Assign Slot"))
		{
			FString SlotTagText = FString(UTF8_TO_TCHAR(SlotTagBuf));
			if (!SlotTagText.IsEmpty() && SelectedStore.IsValid())
			{
				FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*SlotTagText), false);
				if (Tag.IsValid())
				{
					SelectedStore->AddItemToSlot(Item->GetItemId(), Tag);
				}
			}
		}
		if (Item->Slot.IsValid())
		{
			ImGui::SameLine();
			if (ImGui::Button("Remove from Slot"))
			{
				if (SelectedStore.IsValid())
				{
					SelectedStore->RemoveItemFromSlot(Item->GetItemId());
				}
			}
		}

		// Attached items
		if (Item->AttachedItems.Num() > 0)
		{
			ImGui::Spacing();
			ImGui::SeparatorText("Attached Items");
			for (const FArcItemId& AttId : Item->AttachedItems)
			{
				const FArcItemData* Att = SelectedStore->GetItemPtr(AttId);
				if (Att)
				{
					FString AttSlot = Att->AttachedToSlot.IsValid() ? Att->AttachedToSlot.ToString() : TEXT("-");
					ImGui::BulletText("[%s] %s (Slot: %s)",
						TCHAR_TO_ANSI(*AttId.ToString()),
						TCHAR_TO_ANSI(*GetNameSafe(Att->GetItemDefinition())),
						TCHAR_TO_ANSI(*AttSlot));
				}
			}
		}

		// Dynamic Tags
		if (Item->DynamicTags.Num() > 0)
		{
			ImGui::Spacing();
			ImGui::SeparatorText("Dynamic Tags");
			TArray<FGameplayTag> Tags;
			Item->DynamicTags.GetGameplayTagArray(Tags);
			for (const FGameplayTag& Tag : Tags)
			{
				ImGui::BulletText("%s", TCHAR_TO_ANSI(*Tag.ToString()));
			}
		}

		// Aggregated Tags
		if (Item->ItemAggregatedTags.Num() > 0)
		{
			ImGui::Spacing();
			ImGui::SeparatorText("Aggregated Tags");
			TArray<FGameplayTag> Tags;
			Item->ItemAggregatedTags.GetGameplayTagArray(Tags);
			for (const FGameplayTag& Tag : Tags)
			{
				ImGui::BulletText("%s", TCHAR_TO_ANSI(*Tag.ToString()));
			}
		}

		// Definition fragments
		if (Def)
		{
			ImGui::Spacing();
			ImGui::SeparatorText("Definition Fragments");

			const TSet<FArcInstancedStruct>& Fragments = Def->GetFragmentSet();
			for (const FArcInstancedStruct& IS : Fragments)
			{
				if (!IS.IsValid())
				{
					continue;
				}
				ImGui::BulletText("%s", TCHAR_TO_ANSI(*GetNameSafe(IS.GetScriptStruct())));
			}

			const TSet<FArcInstancedStruct>& ScalableFragments = Def->GetScalableFloatFragments();
			if (ScalableFragments.Num() > 0)
			{
				ImGui::Text("  (Scalable Float Fragments)");
				for (const FArcInstancedStruct& IS : ScalableFragments)
				{
					if (!IS.IsValid())
					{
						continue;
					}
					ImGui::BulletText("  %s", TCHAR_TO_ANSI(*GetNameSafe(IS.GetScriptStruct())));
				}
			}
		}

		// Instanced data tree
		ImGui::Spacing();
		DrawInstancedDataTree(Item);
	}
	ImGui::EndChild();
}

void FArcDebuggerItems::DrawInstancedDataTree(const FArcItemData* Item)
{
	if (!Item)
	{
		return;
	}

	ImGui::SeparatorText("Instanced Data");

	if (Item->InstancedData.Num() == 0 && Item->ItemInstances.Data.Num() == 0)
	{
		ImGui::TextDisabled("(none)");
		return;
	}

	const ImGuiTableFlags TableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit;
	if (ImGui::BeginTable("InstancedDataTable", 2, TableFlags))
	{
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_None, 200.f);
		ImGui::TableSetupColumn("Data", ImGuiTableColumnFlags_None, 300.f);
		ImGui::TableHeadersRow();

		// InstancedData map
		for (const auto& Pair : Item->InstancedData)
		{
			ImGui::TableNextRow();

			// Type column
			ImGui::TableSetColumnIndex(0);
			FString TypeName = Pair.Key.ToString();
			if (Pair.Value.IsValid())
			{
				TypeName = GetNameSafe(Pair.Value->GetScriptStruct());
			}

			bool bOpen = ImGui::TreeNode(TCHAR_TO_ANSI(*TypeName));

			// Data column
			ImGui::TableSetColumnIndex(1);
			if (Pair.Value.IsValid())
			{
				ImGui::TextWrapped("%s", TCHAR_TO_ANSI(*Pair.Value->ToString()));
			}
			else
			{
				ImGui::TextDisabled("(null)");
			}

			// Children: struct properties
			if (bOpen)
			{
				if (Pair.Value.IsValid())
				{
					UScriptStruct* Struct = Pair.Value->GetScriptStruct();
					const uint8* Data = reinterpret_cast<const uint8*>(Pair.Value.Get());

					if (Struct && Data)
					{
						for (TFieldIterator<FProperty> PropIt(Struct); PropIt; ++PropIt)
						{
							FProperty* Prop = *PropIt;
							FString ValueStr;
							const void* PropAddr = Prop->ContainerPtrToValuePtr<void>(Data);
							Prop->ExportTextItem_Direct(ValueStr, PropAddr, nullptr, nullptr, PPF_None);

							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0);
							ImGui::Text("  %s", TCHAR_TO_ANSI(*Prop->GetName()));
							ImGui::TableSetColumnIndex(1);
							ImGui::TextWrapped("%s", TCHAR_TO_ANSI(*ValueStr));
						}
					}
				}
				ImGui::TreePop();
			}
		}

		// Replicated ItemInstances
		for (int32 Idx = 0; Idx < Item->ItemInstances.Data.Num(); ++Idx)
		{
			const FArcItemInstanceInternal& Instance = Item->ItemInstances.Data[Idx];
			if (!Instance.IsValid())
			{
				continue;
			}

			ImGui::TableNextRow();

			// Type column
			ImGui::TableSetColumnIndex(0);
			FString RepTypeName = FString::Printf(TEXT("[Rep] %s"), *GetNameSafe(Instance.Data->GetScriptStruct()));
			bool bOpen = ImGui::TreeNode(TCHAR_TO_ANSI(*RepTypeName));

			// Data column
			ImGui::TableSetColumnIndex(1);
			ImGui::TextWrapped("%s", TCHAR_TO_ANSI(*Instance.Data->ToString()));

			if (bOpen)
			{
				UScriptStruct* Struct = Instance.Data->GetScriptStruct();
				const uint8* Data = reinterpret_cast<const uint8*>(Instance.Data.Get());

				if (Struct && Data)
				{
					for (TFieldIterator<FProperty> PropIt(Struct); PropIt; ++PropIt)
					{
						FProperty* Prop = *PropIt;
						FString ValueStr;
						const void* PropAddr = Prop->ContainerPtrToValuePtr<void>(Data);
						Prop->ExportTextItem_Direct(ValueStr, PropAddr, nullptr, nullptr, PPF_None);

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("  %s", TCHAR_TO_ANSI(*Prop->GetName()));
						ImGui::TableSetColumnIndex(1);
						ImGui::TextWrapped("%s", TCHAR_TO_ANSI(*ValueStr));
					}
				}
				ImGui::TreePop();
			}
		}

		ImGui::EndTable();
	}
}

void FArcDebuggerItems::DrawQuickBarTab()
{
	TArray<UArcQuickBarComponent*> QuickBars = GetPlayerQuickBars();

	// QuickBar component selector
	ImGui::Text("QuickBar Component:");
	ImGui::SameLine();

	if (QuickBars.Num() == 0)
	{
		ImGui::TextDisabled("No QuickBar components found on local player");
		return;
	}

	// Auto-select
	if (!SelectedQuickBar.IsValid() && QuickBars.Num() > 0)
	{
		SelectedQuickBarCompIndex = 0;
		SelectedQuickBar = QuickBars[0];
	}

	FString PreviewStr = TEXT("Select QuickBar");
	if (QuickBars.IsValidIndex(SelectedQuickBarCompIndex))
	{
		UArcQuickBarComponent* QB = QuickBars[SelectedQuickBarCompIndex];
		PreviewStr = FString::Printf(TEXT("%s (%s)"), *QB->GetName(), *GetNameSafe(QB->GetOwner()));
	}

	ImGui::SetNextItemWidth(400.f);
	if (ImGui::BeginCombo("##QBSelector", TCHAR_TO_ANSI(*PreviewStr)))
	{
		for (int32 Idx = 0; Idx < QuickBars.Num(); Idx++)
		{
			UArcQuickBarComponent* QB = QuickBars[Idx];
			FString Name = FString::Printf(TEXT("%s (%s)"), *QB->GetName(), *GetNameSafe(QB->GetOwner()));
			if (ImGui::Selectable(TCHAR_TO_ANSI(*Name), SelectedQuickBarCompIndex == Idx))
			{
				SelectedQuickBarCompIndex = Idx;
				SelectedQuickBar = QuickBars[Idx];
				SelectedBarIndex = 0;
			}
		}
		ImGui::EndCombo();
	}

	if (!SelectedQuickBar.IsValid())
	{
		return;
	}

	const TArray<FArcQuickBar>& Bars = SelectedQuickBar->GetQuickBars();

	// Bar selector
	ImGui::Text("Bar:");
	ImGui::SameLine();

	if (Bars.Num() == 0)
	{
		ImGui::TextDisabled("No bars defined");
		return;
	}

	FString BarPreview = Bars.IsValidIndex(SelectedBarIndex) ? Bars[SelectedBarIndex].BarId.ToString() : TEXT("Select Bar");

	ImGui::SetNextItemWidth(250.f);
	if (ImGui::BeginCombo("##BarSelector", TCHAR_TO_ANSI(*BarPreview)))
	{
		for (int32 Idx = 0; Idx < Bars.Num(); Idx++)
		{
			FString BarName = Bars[Idx].BarId.ToString();
			if (ImGui::Selectable(TCHAR_TO_ANSI(*BarName), SelectedBarIndex == Idx))
			{
				SelectedBarIndex = Idx;
			}
		}
		ImGui::EndCombo();
	}

	if (!Bars.IsValidIndex(SelectedBarIndex))
	{
		return;
	}

	const FArcQuickBar& Bar = Bars[SelectedBarIndex];

	// Bar info
	FString ModeStr;
	switch (Bar.QuickSlotsMode)
	{
	case EArcQuickSlotsMode::Cyclable: ModeStr = TEXT("Cyclable"); break;
	case EArcQuickSlotsMode::AutoActivateOnly: ModeStr = TEXT("AutoActivateOnly"); break;
	case EArcQuickSlotsMode::ManualActivationOnly: ModeStr = TEXT("ManualActivationOnly"); break;
	}
	ImGui::Text("Mode: %s | Slots: %d | ItemsStoreClass: %s",
		TCHAR_TO_ANSI(*ModeStr),
		Bar.Slots.Num(),
		TCHAR_TO_ANSI(*GetNameSafe(Bar.ItemsStoreClass)));

	ImGui::Separator();
	DrawQuickBarSlots(SelectedQuickBar.Get(), SelectedBarIndex);
}

void FArcDebuggerItems::DrawQuickBarSlots(UArcQuickBarComponent* QuickBarComp, int32 BarIndex)
{
	if (!QuickBarComp)
	{
		return;
	}

	const TArray<FArcQuickBar>& Bars = QuickBarComp->GetQuickBars();
	if (!Bars.IsValidIndex(BarIndex))
	{
		return;
	}

	const FArcQuickBar& Bar = Bars[BarIndex];
	const FArcSelectedQuickBarSlotList& ReplicatedSlots = QuickBarComp->GetReplicatedSelectedSlots();
	UArcItemsStoreComponent* BarItemStore = QuickBarComp->GetItemStoreComponent(Bar.BarId);

	// Slots table
	const ImGuiTableFlags TableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit ;
	if (ImGui::BeginTable("QBSlotsTable", 6, TableFlags, ImVec2(0.f, 300.f)))
	{
		ImGui::TableSetupColumn("Slot");
		ImGui::TableSetupColumn("ItemSlotId");
		ImGui::TableSetupColumn("Assigned Item");
		ImGui::TableSetupColumn("Active");
		ImGui::TableSetupColumn("Locked");
		ImGui::TableSetupColumn("Actions");
		ImGui::TableHeadersRow();

		for (int32 SlotIdx = 0; SlotIdx < Bar.Slots.Num(); ++SlotIdx)
		{
			const FArcQuickBarSlot& Slot = Bar.Slots[SlotIdx];
			const FGameplayTag& BarId = Bar.BarId;
			const FGameplayTag& SlotId = Slot.QuickBarSlotId;

			ImGui::TableNextRow();
			ImGui::PushID(SlotIdx);

			// Slot ID column
			ImGui::TableSetColumnIndex(0);
			bool bSelected = SelectedQuickBarSlotIndex == SlotIdx;
			if (ImGui::Selectable(TCHAR_TO_ANSI(*SlotId.ToString()), bSelected, ImGuiSelectableFlags_SpanAllColumns))
			{
				SelectedQuickBarSlotIndex = SlotIdx;
			}

			// ItemSlotId column
			ImGui::TableSetColumnIndex(1);
			FString ItemSlotStr = Slot.ItemSlotId.IsValid() ? Slot.ItemSlotId.ToString() : TEXT("-");
			ImGui::Text("%s", TCHAR_TO_ANSI(*ItemSlotStr));

			// Assigned Item column
			ImGui::TableSetColumnIndex(2);
			FArcItemId ItemId = ReplicatedSlots.FindItemId(BarId, SlotId);
			if (ItemId.IsValid())
			{
				const FArcItemData* ItemData = QuickBarComp->FindQuickSlotItem(BarId, SlotId);
				if (ItemData)
				{
					const UArcItemDefinition* Def = ItemData->GetItemDefinition();
					ImGui::Text("%s", TCHAR_TO_ANSI(*GetNameSafe(Def)));
				}
				else
				{
					ImGui::Text("ID: %s", TCHAR_TO_ANSI(*ItemId.ToString()));
				}
			}
			else
			{
				ImGui::TextDisabled("(empty)");
			}

			// Active column
			ImGui::TableSetColumnIndex(3);
			bool bActive = QuickBarComp->IsQuickSlotActive(BarId, SlotId);
			ImGui::Text("%s", bActive ? "Yes" : "No");

			// Locked column
			ImGui::TableSetColumnIndex(4);
			bool bLocked = QuickBarComp->IsQuickSlotLocked(BarId, SlotId);
			ImGui::Text("%s", bLocked ? "Yes" : "No");

			// Actions column
			ImGui::TableSetColumnIndex(5);
			FArcItemId CurrentItemId = ReplicatedSlots.FindItemId(BarId, SlotId);

			if (CurrentItemId.IsValid())
			{
				if (ImGui::SmallButton("Remove"))
				{
					UWorld* World = GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
					if (World)
					{
						Arcx::SendServerCommand<FArcRemoveItemFromQuickSlotCommand>(
							World,
							QuickBarComp,
							BarId,
							SlotId,
							false);
					}
				}
			}
			else if (BarItemStore)
			{
				if (ImGui::SmallButton("Assign..."))
				{
					SelectedQuickBarSlotIndex = SlotIdx;
				}
			}

			ImGui::PopID();
		}

		ImGui::EndTable();
	}

	// Item assignment panel
	if (SelectedQuickBarSlotIndex >= 0 && SelectedQuickBarSlotIndex < Bar.Slots.Num())
	{
		const FArcQuickBarSlot& AssignSlot = Bar.Slots[SelectedQuickBarSlotIndex];
		FArcItemId ExistingItemId = ReplicatedSlots.FindItemId(Bar.BarId, AssignSlot.QuickBarSlotId);

		if (!ExistingItemId.IsValid() && BarItemStore)
		{
			ImGui::Separator();
			ImGui::Text("Assign item to slot: %s", TCHAR_TO_ANSI(*AssignSlot.QuickBarSlotId.ToString()));

			TArray<const FArcItemData*> StoreItems = BarItemStore->GetItems();

			if (StoreItems.Num() == 0)
			{
				ImGui::TextDisabled("No items in the bar's item store");
			}
			else
			{
				ImGui::SetNextItemWidth(200.f);
				ImGui::InputText("Filter##QBAssign", QuickBarAssignFilterBuf, IM_ARRAYSIZE(QuickBarAssignFilterBuf));

				FString LowerFilter = FString(UTF8_TO_TCHAR(QuickBarAssignFilterBuf)).ToLower();

				if (ImGui::BeginChild("AssignList", ImVec2(0.f, 200.f), ImGuiChildFlags_Borders))
				{
					for (int32 i = 0; i < StoreItems.Num(); ++i)
					{
						const FArcItemData* StoreItem = StoreItems[i];
						if (!StoreItem)
						{
							continue;
						}

						const UArcItemDefinition* Def = StoreItem->GetItemDefinition();
						FString DefName = GetNameSafe(Def);

						if (!LowerFilter.IsEmpty() && !DefName.ToLower().Contains(LowerFilter))
						{
							continue;
						}

						bool bAlreadyOnSlot = QuickBarComp->IsItemOnAnyQuickSlot(StoreItem->GetItemId());

						ImGui::PushID(i);
						FString Label = FString::Printf(TEXT("%s (L%d x%d)%s"),
							*DefName,
							StoreItem->GetLevel(),
							StoreItem->GetStacks(),
							bAlreadyOnSlot ? TEXT(" [on slot]") : TEXT(""));

						ImGui::Text("%s", TCHAR_TO_ANSI(*Label));
						ImGui::SameLine();
						if (ImGui::SmallButton("Assign"))
						{
							UWorld* World = GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
							if (World)
							{
								Arcx::SendServerCommand<FArcAddItemToQuickBarCommand>(
									World,
									QuickBarComp,
									BarItemStore,
									StoreItem->GetItemId(),
									Bar.BarId,
									AssignSlot.QuickBarSlotId);
							}
							SelectedQuickBarSlotIndex = -1;
						}
						ImGui::PopID();
					}
				}
				ImGui::EndChild();
			}

			if (ImGui::Button("Close##AssignPanel"))
			{
				SelectedQuickBarSlotIndex = -1;
			}
		}
	}
}
