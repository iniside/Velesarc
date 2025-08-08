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
#include "Commands/ArcAddItemToItemSocketCommand.h"
#include "Commands/ArcMoveItemBetweenStoresCommand.h"

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

	ItemSpecCreator.Initialize();
}

void FArcDebuggerItems::Uninitialize()
{
	ItemDefinitions.Reset();
	ItemList.Reset();
	ItemSpecCreator.Uninitialize();
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
				if (ImGui::BeginPopupContextItem(TCHAR_TO_ANSI(*ItemName))) // <-- use last item id as popup id
				{
					ImGui::Text("This a popup for \"%s\"!", TCHAR_TO_ANSI(*ItemName));
					if (ImGui::Button("Make Item Spec"))
					{
						ImGui::OpenPopup("New Item Spec");
					}
					if (ImGui::BeginPopupModal("New Item Spec"))
					{
						if (ItemsStores.IsValidIndex(SelectedItemStoreIndex))
						{
							UArcItemDefinition* ItemDef = Cast<UArcItemDefinition>(ItemDefinitions[Idx].GetAsset());
							ItemSpecCreator.CreateItemSpec(ItemsStores[SelectedItemStoreIndex], ItemDef);	
						}
												
						if (ImGui::Button("Close"))
						{
							ImGui::CloseCurrentPopup();
						}
						
						if (ImGui::Button("Add Item"))
						{
							Arcx::SendServerCommand<FArcAddItemSpecCommand>(PC, ItemsStores[SelectedItemStoreIndex], ItemSpecCreator.TempNewSpec);
						}
						ImGui::EndPopup();
					}
					ImGui::EndPopup();
				}
				
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
						if (ItemData->GetOwnerId().IsValid())
						{
							continue;
						}
						
						ImGui::PushID(TCHAR_TO_ANSI(*ItemData->GetItemId().ToString()));
						if (ImGui::TreeNode(TCHAR_TO_ANSI(*GetNameSafe(ItemData->GetItemDefinition()))))
						{
							for (int32 StoreIdx = 0; StoreIdx < ItemsStores.Num(); StoreIdx++)
							{
								if (ItemsStore == ItemsStores[StoreIdx])
								{
									continue; // Skip the same store
								}
								
								if (ImGui::Button(TCHAR_TO_ANSI(*FString::Printf(TEXT("Move to %s"), *ItemsStores[StoreIdx]->GetName()))))
								{
									Arcx::SendServerCommand<FArcMoveItemBetweenStoresCommand>(PC, ItemsStore, ItemsStores[StoreIdx], ItemData->GetItemId(), 1);
								}
							}
							
							ImGui::Text("ItemId: "); ImGui::SameLine();
							ImGui::Text(TCHAR_TO_ANSI(*ItemData->GetItemId().ToString()));

							ImGui::Text("OwnerId: "); ImGui::SameLine();
							ImGui::Text(TCHAR_TO_ANSI(*ItemData->GetOwnerId().ToString()));

							ImGui::Text("Stacks: "); ImGui::SameLine();
							ImGui::Text("%d", ItemData->GetStacks());
							
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
										FArcItemInstanceDebugger::DrawAbilityEffectsToApply(InItemData, Instance);
										FArcItemInstanceDebugger::DrawGrantedPassiveAbilities(InItemData, Instance);
										FArcItemInstanceDebugger::DrawGrantedEffects(InItemData, Instance);
										FArcItemInstanceDebugger::DrawItemStats(InItemData, Instance);
										ImGui::TreePop();
									}
								});

								ImGui::TreePop();
							}
							
							const FArcItemFragment_SocketSlots* SocketSlotsFragment = ArcItems::FindFragment<FArcItemFragment_SocketSlots>(ItemData);
							if (SocketSlotsFragment)
							{
								if (ImGui::TreeNode("Attached Items"))
								{
									const TArray<FArcSocketSlot>& SocketSlots = SocketSlotsFragment->GetSocketSlots();
									const TArray<FArcItemId>& AttachedItems = ItemData->GetAttachedItems();
									for (const FArcSocketSlot& SocketSlot : SocketSlots)
									{
										if (ImGui::TreeNode(TCHAR_TO_ANSI(*SocketSlot.SlotId.ToString())))
										{
											const FArcItemData* AttachedItem = nullptr;
											for (const FArcItemId& AttachedItemId : AttachedItems)
											{
												const FArcItemData* ItemDataPtr = ItemsStore->GetItemPtr(AttachedItemId);
												if (ItemDataPtr && ItemDataPtr->GetAttachSlot() == SocketSlot.SlotId)
												{
													AttachedItem = ItemDataPtr;
													break;
												}
											}

											TArray<const FArcItemData*> AvailableAttachments;
											for (const FArcItemData* Item : StoreItems)
											{
												if (AttachedItem && (Item->GetItemId() == AttachedItem->GetItemId()))
												{
													continue; // Skip the item itself
												}
												const FArcItemFragment_Tags* Tags = ArcItems::FindFragment<FArcItemFragment_Tags>(Item);
												if (Tags && Tags->ItemTags.HasAll(SocketSlot.RequiredItemTags))
												{
													AvailableAttachments.Add(Item);
												}
											}
											
											if (AttachedItem)
											{
												if (AttachedItem->GetItemDefinition())
												{
													if (ImGui::Button("Detach Item"))
													{
														Arcx::SendServerCommand<FArcRemoveItemFromItemSocketCommand>(PC, ItemsStore, AttachedItem->GetItemId());
													}
													
													ImGui::Text("Attached Item: "); ImGui::SameLine();
													ImGui::Text(TCHAR_TO_ANSI(*AttachedItem->GetItemDefinition()->GetName()));

													FString ItemIdStr = FString::Printf(TEXT("ItemId: %s"), *AttachedItem->GetItemId().ToString());
													ImGui::Text(TCHAR_TO_ANSI(*ItemIdStr));
												}
											}

											FString PreviewItemName = "Select Attachment";
											if (AttachedItem)
											{
												PreviewItemName = AttachedItem->GetItemDefinition()->GetName();
											}
											
											if (ImGui::BeginCombo("Item List", TCHAR_TO_ANSI(*PreviewItemName)))
											{
												for (int32 ItemIdx = 0; ItemIdx < AvailableAttachments.Num(); ItemIdx++)
												{
													const FArcItemData* AvailableItem = AvailableAttachments[ItemIdx];
													if (AvailableItem && AvailableItem->GetItemDefinition())
													{
														if (ImGui::Selectable(TCHAR_TO_ANSI(*AvailableItem->GetItemDefinition()->GetName())))
														{
															Arcx::SendServerCommand<FArcAddItemToItemSocketCommand>(PC, ItemsStore, ItemData->GetItemId(), AvailableItem->GetItemId(), SocketSlot.SlotId);
														}
													}
												}
												ImGui::EndCombo();
											}
											
											ImGui::TreePop();
										}
									}
									
									ImGui::TreePop();
								}
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