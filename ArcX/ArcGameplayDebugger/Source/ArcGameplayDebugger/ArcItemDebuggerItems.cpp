#include "ArcItemDebuggerItems.h"

#include "ArcGameplayDebuggerModule.h"
#include "ArcItemInstanceDataDebugger.h"
#include "SlateIM.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Commands/ArcEquipItemCommand.h"
#include "Commands/ArcReplicatedCommandHelpers.h"
#include "Engine/AssetManager.h"
#include "Equipment/ArcEquipmentComponent.h"
#include "Equipment/ArcItemAttachmentComponent.h"
#include "GameFramework/PlayerState.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/Fragments/ArcItemFragment_SocketSlots.h"
#include "Kismet/GameplayStatics.h"
#include "QuickBar/ArcQuickBarComponent.h"
#include "imgui.h"
#include "Commands/ArcAddItemSpecCommand.h"
#define LOCTEXT_NAMESPACE "FArcGameplayDebuggerModule"

void FArcDebuggerItems::Initialize()
{
	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	if (!AssetManager)
	{
		return;
	}
	
	IAssetRegistry& AR = AssetManager->GetAssetRegistry();

	AR.GetAssetsByClass(UArcItemDefinition::StaticClass()->GetClassPathName(), ItemDefinitions);

	for (const FAssetData& ItemData : ItemDefinitions)
	{
		if (ItemData.IsValid())
		{
			ItemList.Add(ItemData.AssetName.ToString());	
		}
	}
}

void FArcDebuggerItems::Uninitialize()
{
	ItemDefinitions.Reset();
	ItemList.Reset();
}

void FArcDebuggerItems::Draw()
{
	if (!GEngine)
	{
		return;
	}
	
	if (!GEngine->GameViewport)
	{
		return;
	}

	UWorld* World = GEngine->GameViewport->GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC)
	{
		return;
	}

	if (!PC->PlayerState)
	{
		return;
	}

	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	if (!AssetManager)
	{
		return;
	}

	APlayerState* PS = PC->PlayerState;

	UArcCoreAbilitySystemComponent* AbilitySystem = PS->FindComponentByClass<UArcCoreAbilitySystemComponent>();
	TArray<UArcItemsStoreComponent*> ItemsStores;
	PS->GetComponents<UArcItemsStoreComponent>(ItemsStores);
                      						
	// TODO:: Equip new
	// TODO:: Equip from items store.
	TArray<UArcEquipmentComponent*> EquipmentComponents;
	PS->GetComponents<UArcEquipmentComponent>(EquipmentComponents);

	UArcQuickBarComponent* QuickBarComponent = PS->FindComponentByClass<UArcQuickBarComponent>();

	UArcItemAttachmentComponent* ItemAttachmentComponent = PS->FindComponentByClass<UArcItemAttachmentComponent>();
	
	TArray<FString> ItemStoreNames;
	Algo::Transform(ItemsStores, ItemStoreNames, [](const UArcItemsStoreComponent* Store){
		return Store ? FString::Printf(TEXT("Add to %s"), *Store->GetName()) : FString(TEXT("Invalid Store"));
	});
	
	IAssetRegistry& AR = AssetManager->GetAssetRegistry();

	ImGui::Begin("Items");

	if (ImGui::BeginTable("LayoutTable", 2))
	{
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		FString PreviewValue = "Select Item Store";
		if (ItemStoreNames.IsValidIndex(SelectedItemStoreIndex))
		{
			PreviewValue = ItemStoreNames[SelectedItemStoreIndex];
		}
		if (ImGui::BeginCombo("Select Item Store", TCHAR_TO_ANSI(*PreviewValue)))
		{
			for (int32 Idx = 0; Idx < ItemStoreNames.Num(); Idx++)
			{
				if (ImGui::Selectable(TCHAR_TO_ANSI(*ItemStoreNames[Idx]), SelectedItemStoreIndex == Idx))
				{
					SelectedItemStoreIndex = Idx;
				}
			}
			ImGui::EndCombo();
		}
		
		if (ImGui::BeginChild("ItemsListChild", ImVec2(0.f, 400.f)))
		{
			ImGui::BeginTable("ItemsTable", 2);
			for (int32 Idx = 0; Idx < ItemList.Num(); Idx++)
			{
				const FString& ItemName = ItemList[Idx];
				if (ItemName.IsEmpty())
				{
					continue;
				}
			
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text(TCHAR_TO_ANSI(*ItemName));
			
				ImGui::TableSetColumnIndex(1);
				ImGui::PushID(TCHAR_TO_ANSI(*ItemName));
				if (ImGui::Button("Add Item"))
				{
					if (ItemsStores.IsValidIndex(SelectedItemStoreIndex))
					{
						FPrimaryAssetId AssetId = AssetManager->GetPrimaryAssetIdForData(ItemDefinitions[Idx]);
						FArcItemSpec Spec;
						Spec.SetItemDefinition(AssetId);
						
						Arcx::SendServerCommand<FArcAddItemSpecCommand>(PC, ItemsStores[SelectedItemStoreIndex], Spec);
					}
				}
				ImGui::PopID();
			}
			ImGui::EndTable();
			ImGui::EndChild();
		}
	
		if (ImGui::BeginChild("ItemsListChild2", ImVec2(0.f, 400.f)))
		{
			ImGui::BeginTable("ItemsTable", 2);
			for (const FString& ItemName : ItemList)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text(TCHAR_TO_ANSI(*ItemName));
				
				ImGui::TableSetColumnIndex(1);
				ImGui::PushID(TCHAR_TO_ANSI(*ItemName));
				if (ImGui::Button("Add Item"))
				{
				}
				ImGui::PopID();
			}
			ImGui::EndTable();
			ImGui::EndChild();
		}
	
		ImGui::TableSetColumnIndex(1);
		if (ImGui::BeginChild("ItemsStoreListChild"))
		{
			for (UArcItemsStoreComponent* ItemsStore : ItemsStores)
			{
				if (!ItemsStore)
				{
					continue;
				}
		
				if (ImGui::TreeNode(TCHAR_TO_ANSI(*GetNameSafe(ItemsStore))))
				{
					TArray<const FArcItemData*> StoreItems = ItemsStore->GetItems();
					for (const FArcItemData* ItemData : StoreItems)
					{
						ImGui::PushID(TCHAR_TO_ANSI(*ItemData->GetItemId().ToString()));
						if (ImGui::TreeNode(TCHAR_TO_ANSI(*GetNameSafe(ItemData->GetItemDefinition()))))
						{
							ImGui::Text("ItemId: "); ImGui::SameLine();
							ImGui::Text(TCHAR_TO_ANSI(*ItemData->GetItemId().ToString()));

							ImGui::Text("OwnerId: "); ImGui::SameLine();
							ImGui::Text(TCHAR_TO_ANSI(*ItemData->GetOwnerId().ToString()));

							ImGui::Text("Slot: "); ImGui::SameLine();
							ImGui::Text(TCHAR_TO_ANSI(*ItemData->GetSlotId().ToString()));

							ImGui::Text("AttachedToSlot: "); ImGui::SameLine();
							ImGui::Text(TCHAR_TO_ANSI(*ItemData->GetAttachSlot().ToString()));

							if (ImGui::TreeNode("Instance Data"))
							{
								ArcItems::ForEachInstance<FArcItemInstance>(ItemData, [](const FArcItemData* InItemData, const FArcItemInstance* Instance)
								{
									if (ImGui::TreeNode(TCHAR_TO_ANSI(*Instance->GetScriptStruct()->GetName())))
									{
										FArcItemInstanceDebugger::DrawGrantedAbilities(InItemData, Instance);
										ImGui::TreePop();
									}
								});

								ImGui::TreePop();
							}
							if (ImGui::TreeNode("Attached Items"))
							{
								const TArray<FArcItemId>& AttachedItemId = ItemData->GetAttachedItems();
								for (const FArcItemId& AttachedItem : AttachedItemId)
								{
									const FArcItemData* AttachedItemData = ItemsStore->GetItemPtr(AttachedItem);
									if (AttachedItemData && AttachedItemData->GetItemDefinition())
									{
										ImGui::PushID(TCHAR_TO_ANSI(*AttachedItemData->GetItemId().ToString()));
										if (ImGui::TreeNode(TCHAR_TO_ANSI(*AttachedItemData->GetItemDefinition()->GetName())))
										{
											ImGui::TreePop();
										}
										ImGui::PopID();
									}
								}
								ImGui::TreePop();
							}
							
							ImGui::TreePop();		
						}
						ImGui::PopID();
					}
					ImGui::TreePop();
				}
			}
			ImGui::EndChild();
		}
		ImGui::EndTable();
	}
	
	ImGui::End();
}

void FArcItemDebuggerItems::Initialize()
{
}

void FArcItemDebuggerItems::Uninitialize()
{
}


void MakeAttachmentTree(UArcItemsStoreComponent* ItemsStore, const FArcItemData* ItemData)
{
	if (!ItemsStore || !ItemData)
	{
		return;
	}

	const TArray<FArcItemId>& AttachedItems = ItemData->GetAttachedItems();
	if (AttachedItems.Num() > 0)
	{
		if (SlateIM::NextTableCell())
		{
			SlateIM::Text(TEXT("Attached Items"));
		}
		
		if (SlateIM::BeginTableRowChildren())
		{
			for (const FArcItemId& ItemId : AttachedItems)
			{
				const FArcItemData* NewItemData = ItemsStore->GetItemPtr(ItemId);
				if (NewItemData && NewItemData->GetItemDefinition())
				{
					SlateIM::BeginVerticalStack();
					SlateIM::Text(FString::Printf(TEXT("%s : %s"), *NewItemData->GetItemDefinition()->GetName(), *NewItemData->GetItemId().ToString()));
					SlateIM::Text(FString::Printf(TEXT("QuickSlotId: %s"), *NewItemData->GetSlotId().ToString()));
					
					if (SlateIM::Button(TEXT("Equip")))
					{
						UE_LOG(LogTemp, Log, TEXT("Equipping item %s"), *NewItemData->GetItemId().ToString());
					}					
					SlateIM::EndVerticalStack();
				}
				if (NewItemData->GetAttachedItems().Num() > 0)
				{
					MakeAttachmentTree(ItemsStore, NewItemData);
				}
			}
		}
		SlateIM::EndTableRowChildren();
	}
};

void FArcItemDebuggerItems::Draw()
{

	if (!GEngine)
	{
		return;
	}
	
	if (!GEngine->GameViewport)
	{
		return;
	}

	UWorld* World = GEngine->GameViewport->GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC)
	{
		return;
	}

	if (!PC->PlayerState)
	{
		return;
	}

	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	if (!AssetManager)
	{
		return;
	}

	APlayerState* PS = PC->PlayerState;

	UArcCoreAbilitySystemComponent* AbilitySystem = PS->FindComponentByClass<UArcCoreAbilitySystemComponent>();
	TArray<UArcItemsStoreComponent*> ItemsStores;
	PS->GetComponents<UArcItemsStoreComponent>(ItemsStores);
                      						
	// TODO:: Equip new
	// TODO:: Equip from items store.
	TArray<UArcEquipmentComponent*> EquipmentComponents;
	PS->GetComponents<UArcEquipmentComponent>(EquipmentComponents);

	UArcQuickBarComponent* QuickBarComponent = PS->FindComponentByClass<UArcQuickBarComponent>();

	UArcItemAttachmentComponent* ItemAttachmentComponent = PS->FindComponentByClass<UArcItemAttachmentComponent>();
	
	TArray<FString> ItemStoreNames;
	Algo::Transform(ItemsStores, ItemStoreNames, [](const UArcItemsStoreComponent* Store){
		return Store ? FString::Printf(TEXT("Add to %s"), *Store->GetName()) : FString(TEXT("Invalid Store"));
	});
	
	IAssetRegistry& AR = AssetManager->GetAssetRegistry();

	TArray<FAssetData> ItemDefinitions;
	AR.GetAssetsByClass(UArcItemDefinition::StaticClass()->GetClassPathName(), ItemDefinitions);

	TArray<FString> ItemList;
	for (const FAssetData& ItemData : ItemDefinitions)
	{
		if (ItemData.IsValid())
		{
			ItemList.Add(ItemData.AssetName.ToString());	
		}
	}
	const FTableRowStyle* TableRowStyle = &FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.AlternatingRow");
	SlateIM::AutoSize();
	
	SlateIM::HAlign(HAlign_Fill);
	SlateIM::VAlign(VAlign_Fill);
	SlateIM::BeginTabGroup("Debugger");
	
	SlateIM::BeginTabSplitter(Orient_Horizontal);
	SlateIM::BeginTabStack();
	SlateIM::BeginTab("ItemsDebuggerListTabV1", FSlateIcon(), LOCTEXT("ItemsDebuggerListTab", "Items"));
	{
		SlateIM::BeginTable(nullptr, TableRowStyle);
			
		SlateIM::AddTableColumn(TEXT("Item"));
		SlateIM::AddTableColumn(TEXT("Actions"));
		for (const FString& ItemName : ItemList)
		{
			if (SlateIM::NextTableCell())
			{
				SlateIM::Padding(FMargin(5, 0));
				SlateIM::VAlign(VAlign_Center);// Centers the button in the row
				SlateIM::Text(ItemName);;
				SlateIM::Padding(FMargin(0));
			}
			if (SlateIM::NextTableCell())
			{
				SlateIM::BeginHorizontalStack();
				if (SlateIM::Button(TEXT("Add Item")))
				{
				}
				SlateIM::EndHorizontalStack();
			}
		}

		SlateIM::EndTable();
	}
	SlateIM::EndTab();
	SlateIM::BeginTab("ItemsDebuggerItemsStoresListV1", FSlateIcon(), LOCTEXT("ItemsDebuggerItemsStoresList", "Items"));
	{
		SlateIM::BeginTable(nullptr, TableRowStyle);
			
		SlateIM::AddTableColumn(TEXT(""));
		for (UArcItemsStoreComponent* ItemsStore : ItemsStores)
		{
			if (!ItemsStore)
			{
				continue;
			}
			if (SlateIM::NextTableCell())
			{
				SlateIM::Text(GetNameSafe(ItemsStore));
			}
			if (SlateIM::BeginTableRowChildren())
			{
				TArray<const FArcItemData*> StoreItems = ItemsStore->GetItems();

				for (const FArcItemData* ItemData : StoreItems)
				{
					if (SlateIM::NextTableCell())
					{
						SlateIM::Text(GetNameSafe(ItemData->GetItemDefinition()));
					}		
				}
			}
			SlateIM::EndTableRowChildren();
		}
		SlateIM::EndTable();
	}
	SlateIM::EndTab();
	SlateIM::EndTabStack();
	SlateIM::EndTabSplitter();
	SlateIM::EndTabGroup();
#if 0
	SlateIM::BeginTable(nullptr, TableRowStyle);
			
	SlateIM::AddTableColumn(TEXT("ItemList"));
	SlateIM::AddTableColumn(TEXT("Editor"));
	
	if (SlateIM::NextTableCell())
	{
		SlateIM::Fill();
		SlateIM::VAlign(VAlign_Fill);
		SlateIM::HAlign(HAlign_Fill);
		SlateIM::MaxHeight(600.f);
		SlateIM::BeginTable(nullptr, TableRowStyle);
			
		SlateIM::AddTableColumn(TEXT("ItemList"));
		SlateIM::AddTableColumn(TEXT("Editor"));
		
		{
			for (const FString& ItemName : ItemList)
			{
				if (SlateIM::NextTableCell())
				{
					SlateIM::Padding(FMargin(5, 0));
					SlateIM::VAlign(VAlign_Center);// Centers the button in the row
					SlateIM::Text(ItemName);;
					SlateIM::Padding(FMargin(0));
				}
				if (SlateIM::NextTableCell())
				{
					int32 SelectedItemStoreIdx = -1;
					if (SlateIM::ComboBox(ItemStoreNames, SelectedItemStoreIdx))
					{
						SelectedItemStoreIdx = -1;
					}
				}
			}
		}
		
		SlateIM::EndTable();
	}
	
	if (SlateIM::NextTableCell())
	{
		for (UArcEquipmentComponent* EquipmentComponent : EquipmentComponents)
		{
			if (!EquipmentComponent)
			{
				continue;
			}
		
			SlateIM::Text(EquipmentComponent->GetName());

			struct ItemData
			{
				FArcItemId ItemId;
				FString ItemName;
			};

			TArray<FGameplayTag> EquipmentSlots = EquipmentComponent->GetEquipmentSlots();

			TArray<const FArcItemData*> StoreItems = ItemsStores[0]->GetItems();

			TArray<FString> CurrentItemList;
			TArray<FArcItemId> CurrentItemIds;
			for (const FArcItemData* StoreItem : StoreItems)
			{
				if (StoreItem && StoreItem->GetItemDefinition())
				{
					CurrentItemList.Add(GetNameSafe(StoreItem->GetItemDefinition()));
					CurrentItemIds.Add(StoreItem->GetItemId());
				}
			}
			
			SlateIM::Fill();
			SlateIM::AutoSize();
			SlateIM::VAlign(VAlign_Fill);
			SlateIM::HAlign(HAlign_Fill);
			SlateIM::MaxHeight(400.f);
			SlateIM::BeginTable();
			SlateIM::AddTableColumn(TEXT("Equipment"));
			for (const FGameplayTag& EquipmentSlot : EquipmentSlots)
			{
				TArray<ItemData> ItemOptions;
				TArray<FString> OptionsNames;
			
				if (ItemsStores.Num() > 0)
				{
					TArray<const FArcItemData*> Items = EquipmentComponent->GetItemsFromStoreForSlot(ItemsStores[0], EquipmentSlot);

					for (const FArcItemData* ItemData : Items)
					{
						if (ItemData && ItemData->GetItemDefinition())
						{
							ItemOptions.Add({ ItemData->GetItemId(), ItemData->GetItemDefinition()->GetName() });
							OptionsNames.Add(ItemData->GetItemDefinition()->GetName());
						}
					}
				}

				const FArcItemData* EquippedItem = ItemsStores[0]->GetItemFromSlot(EquipmentSlot);
				if (SlateIM::NextTableCell())
				{
					FString EquippedItemName = EquippedItem ? GetNameSafe(EquippedItem->GetItemDefinition()) : TEXT("No Item Equipped");

					FString SlotLocked = ItemsStores[0]->IsSlotLocked(EquipmentSlot) ? TEXT(" (Locked)") : TEXT("(Unlocked)");
					SlateIM::HAlign(HAlign_Fill);
					SlateIM::VAlign(VAlign_Center);
					SlateIM::BeginHorizontalStack();

					SlateIM::Text(FString::Printf(TEXT("%s : %s : %s"), *EquipmentSlot.ToString(), *EquippedItemName, *SlotLocked));
					SlateIM::Button(TEXT("Unequip"));

					SlateIM::EndHorizontalStack();
				}

				if (SlateIM::BeginTableRowChildren())
				{
					if (SlateIM::NextTableCell())
					{
						SlateIM::Text(TEXT("Items To Equip"));
					}
					if (SlateIM::BeginTableRowChildren())
					{
						int32 SelectedItemIdx = -1;
						if (EquippedItem)
						{
							for (int32 Idx = 0; Idx < ItemOptions.Num(); ++Idx)
							{
								if (ItemOptions[Idx].ItemId == EquippedItem->GetItemId())
								{
									SelectedItemIdx = Idx;
									break;
								}
							}
						}
						
						if (SlateIM::SelectionList(OptionsNames, SelectedItemIdx))
						{
							Arcx::SendServerCommand<FArcEquipItemCommand>(World, ItemsStores[0], EquipmentComponent
								, ItemOptions[SelectedItemIdx].ItemId, EquipmentSlot);
						}
						
						if (SlateIM::NextTableCell())
						{
							SlateIM::Text(TEXT("Attachments: "));
						}
						if (SlateIM::BeginTableRowChildren())
						{
							if (EquippedItem)
							{
								const FArcItemFragment_SocketSlots* AttachmentSlots = ArcItems::FindFragment<FArcItemFragment_SocketSlots>(EquippedItem);
								if (AttachmentSlots)
								{
									for (const FArcSocketSlot& AttachSlot : AttachmentSlots->GetSocketSlots())
									{
										if (SlateIM::NextTableCell())
										{
											SlateIM::Text(AttachSlot.SlotId.ToString());
										}
										
										if (SlateIM::BeginTableRowChildren())
										{
											TArray<FString> FilteredAttachmentList;
											TArray<FArcItemId> FilteredAttachmentIds;
											for (const FArcItemData* ItemData : StoreItems)
											{
												const FArcItemFragment_Tags* Tags = ArcItems::FindFragment<FArcItemFragment_Tags>(ItemData);
												if (!Tags)
												{
													continue;
												}

												if (Tags->ItemTags.HasAll(AttachSlot.RequiredItemTags))
												{
													FilteredAttachmentList.Add(GetNameSafe(ItemData->GetItemDefinition()));
													FilteredAttachmentIds.Add(ItemData->GetItemId());
												}
											}
											int32 SelectedAttachmentIdx = -1;
											const FArcItemData* AttachedItem = ItemsStores[0]->GetItemAttachedTo(EquippedItem->GetItemId(), AttachSlot.SlotId);
											if (AttachedItem)
											{
												for (int32 Idx = 0; Idx < FilteredAttachmentIds.Num(); ++Idx)
												{
													if (FilteredAttachmentIds[Idx] == AttachedItem->GetItemId())
													{
														SelectedAttachmentIdx = Idx;
														break;
													}
												}
											}
											SlateIM::SelectionList(FilteredAttachmentList, SelectedAttachmentIdx);
										}
										SlateIM::EndTableRowChildren();
									}
								}
							}
						}
						SlateIM::EndTableRowChildren();
					}
					SlateIM::EndTableRowChildren();
				}
				SlateIM::EndTableRowChildren();
			}
			SlateIM::EndTable();
		}
	
		SlateIM::AutoSize();

		SlateIM::HAlign(HAlign_Fill);
		SlateIM::VAlign(VAlign_Fill);
		SlateIM::MaxHeight(500.f);
		
		SlateIM::BeginTable(nullptr, TableRowStyle);
	
		SlateIM::AddTableColumn(TEXT("Name"));

		for (UArcItemsStoreComponent* ItemsStore : ItemsStores)
		{
			if (ItemsStore)
			{
				if (SlateIM::NextTableCell())
				{
					SlateIM::HAlign(HAlign_Fill);
					SlateIM::VAlign(VAlign_Center);
					SlateIM::Text(ItemsStore->GetName());
				}

				if (SlateIM::BeginTableRowChildren())
				{
					TArray<const FArcItemData*> Items = ItemsStore->GetItems();

					TArray<FString> CurrentItemList;
					TArray<FArcItemId> CurrentItemIds;
					for (const FArcItemData* ItemData : Items)
					{
						if (ItemData && ItemData->GetItemDefinition())
						{
							CurrentItemList.Add(GetNameSafe(ItemData->GetItemDefinition()));
							CurrentItemIds.Add(ItemData->GetItemId());
						}
					}
					
					for (const FArcItemData* ItemData : Items)
					{
						if (ItemData->OwnerId.IsValid())
						{
							continue;
						}
					
						if (ItemData && ItemData->GetItemDefinition())
						{
							if (SlateIM::NextTableCell())
							{
								SlateIM::AutoSize();
								SlateIM::HAlign(HAlign_Fill);
								SlateIM::VAlign(VAlign_Center);
								SlateIM::BeginContextMenuAnchor();
								
								SlateIM::BeginHorizontalStack();
								SlateIM::Text(FString::Printf(TEXT("%s : %s"), *GetNameSafe(ItemData->GetItemDefinition()), *ItemData->GetItemId().ToString()));
								if (SlateIM::Button(TEXT("Remove")))
								{
									//Arcx::SendServerCommand<FArcRemoveItemCommand>(World, ItemsStore, ItemData->GetItemId());
								}
								SlateIM::EndHorizontalStack();
								
								SlateIM::AddMenuSection(TEXT("ItemActions"));

								SlateIM::AddMenuSeparator();

								SlateIM::AddMenuButton(FString::Printf(TEXT("Drop %s"), *GetNameSafe(ItemData->GetItemDefinition())));
								SlateIM::AddMenuButton(FString::Printf(TEXT("Remove %s"), *GetNameSafe(ItemData->GetItemDefinition())));

								SlateIM::AddMenuSeparator();

								{
									SlateIM::BeginSubMenu(TEXT("Add To QuickSlot"));
									if (QuickBarComponent)
									{
										const TArray<FArcQuickBar>& QuickBars =  QuickBarComponent->GetQuickBars();
										for (const FArcQuickBar& QuickBar : QuickBars)
										{
											for (const FArcQuickBarSlot& QuickSlot : QuickBar.Slots)
											{
												if (SlateIM::AddMenuButton(FString::Printf(TEXT("%s -> %s"), *GetNameSafe(ItemData->GetItemDefinition()), *QuickSlot.QuickBarSlotId.ToString())))
												{
													//	Arcx::SendServerCommand<FArcAddItemToQuickBarCommand>(World, QuickBarComponent, ItemData->GetItemId(), QuickSlot.SlotName);
												}
											}
										}
									}
									SlateIM::EndSubMenu();
								}
								SlateIM::AddMenuSeparator();
								{
									SlateIM::BeginSubMenu(TEXT("Move To"));
									for (UArcItemsStoreComponent* Store : ItemsStores)
									{
										if (Store != ItemsStore)
										{
											SlateIM::AddMenuButton(FString::Printf(TEXT("%s To %s"), *GetNameSafe(ItemData->GetItemDefinition()), *GetNameSafe(Store)));
										}
									}
									SlateIM::EndSubMenu();
								}
								
								SlateIM::EndContextMenuAnchor();
							}
							if (SlateIM::BeginTableRowChildren())
							{
								SlateIM::BeginVerticalStack();
								{
									FGameplayTag SlotId = ItemData->GetSlotId();
								
									SlateIM::Text(FString::Printf(TEXT("QuickSlotId: %s"), *SlotId.ToString()));
								
									FGameplayTag AttachSlot = ItemData->GetAttachSlot();
								
									SlateIM::Text(FString::Printf(TEXT("AttachSlotId: %s"), *AttachSlot.ToString()));
								}
								SlateIM::EndVerticalStack();
								if (SlateIM::NextTableCell())
								{
									SlateIM::Text(TEXT("Instance Data"));
								}
								if (SlateIM::BeginTableRowChildren())
								{
									ArcItems::ForEachInstance<FArcItemInstance>(ItemData, [&](const FArcItemData* InData, FArcItemInstance* InInstance)
									{
										if (auto Func = FArcGameplayDebuggerTools::ItemInstancesDraw.Find(InInstance->GetScriptStruct()->GetFName()))
										{
											(*Func)(InData, InInstance);
										}
									});
								}
								SlateIM::EndTableRowChildren();
								
								const FArcItemFragment_SocketSlots* SocketSlots = ArcItems::FindFragment<FArcItemFragment_SocketSlots>(ItemData);
								if (SocketSlots)
								{
									if (SlateIM::NextTableCell())
									{
										SlateIM::Text(TEXT("AttachmentSlots: "));
									}
									if (SlateIM::BeginTableRowChildren())
									{
										SlateIM::AutoSize();
										SlateIM::HAlign(HAlign_Fill);
										SlateIM::VAlign(VAlign_Fill);
										
										for (const auto& SocketSlot : SocketSlots->GetSocketSlots())
										{
											if (SlateIM::NextTableCell())
											{
												SlateIM::Text(FString::Printf(TEXT("QuickSlotId %s"), *SocketSlot.SlotId.ToString()));
												SlateIM::Button(TEXT("Detach"));
											}
											if (SlateIM::BeginTableRowChildren())
											{
												const FArcItemData* AttachedItem = ItemsStore->FindAttachedItemOnSlot(ItemData->GetItemId(), SocketSlot.SlotId);
												if (SlateIM::NextTableCell())
												{
													SlateIM::Text(FString::Printf(TEXT("ItemType: %s"), *SocketSlot.ItemType.ToString()));
												}
												if (SlateIM::NextTableCell())
												{
													SlateIM::Text(FString::Printf(TEXT("RequiredItemTags: %s"), *SocketSlot.RequiredItemTags.ToStringSimple()));
												}
												if (SlateIM::NextTableCell())
												{
													SlateIM::Text(FString::Printf(TEXT("SlotName: %s"), *SocketSlot.SlotName.ToString()));
												}
												if (SlateIM::NextTableCell())
												{
													SlateIM::Text(FString::Printf(TEXT("DefaultSocketItemDefinition: %s"), *GetNameSafe(SocketSlot.DefaultSocketItemDefinition)));
												}
												if (SlateIM::NextTableCell())
												{
													FString AttachedItemName = AttachedItem ? GetNameSafe(AttachedItem->GetItemDefinition()) : TEXT("None");
													SlateIM::Text(FString::Printf(TEXT("AttachedItem: %s"), *AttachedItemName));
												}
												if (SlateIM::BeginTableRowChildren())
												{
													SlateIM::BeginHorizontalStack();

													SlateIM::AutoSize();
													SlateIM::MaxHeight(150.f);
													SlateIM::BeginScrollBox();
													{
														int32 SelectedItemIndex = -1;
														for (int32 Idx = 0; Idx < CurrentItemIds.Num(); ++Idx)
														{
															if (AttachedItem && AttachedItem->GetItemId() == CurrentItemIds[Idx])
															{
																SelectedItemIndex = Idx;
																break;
															}
														}
														SlateIM::SelectionList(CurrentItemList, SelectedItemIndex);
													}
													SlateIM::EndScrollBox();

													SlateIM::AutoSize();
													SlateIM::MaxHeight(150.f);
													SlateIM::BeginScrollBox();
													{
														int32 SelectedItemIndex = -1;
														SlateIM::SelectionList(ItemList, SelectedItemIndex);
													}
													SlateIM::EndScrollBox();
													
													SlateIM::EndHorizontalStack();
												}
												SlateIM::EndTableRowChildren();
												
											}
											SlateIM::EndTableRowChildren();
										}
									}
									SlateIM::EndTableRowChildren();
								}
								MakeAttachmentTree(ItemsStore, ItemData);
							}
							SlateIM::EndTableRowChildren();
							
						}
					}
				}
				SlateIM::EndTableRowChildren();
			}
		}
		SlateIM::EndTable();
	}
	
	SlateIM::EndTable();
#endif
}
#undef LOCTEXT_NAMESPACE