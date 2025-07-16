/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "ArcGameplayDebuggerModule.h"

#if WITH_EDITOR
#include "SLevelViewport.h"
#endif

#include "ArcCoreUtils.h"
#include "ArcItemInstanceDataDebugger.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"
#include "Commands/ArcAddItemToQuickBarCommand.h"
#include "Commands/ArcEquipItemCommand.h"
#include "Commands/ArcReplicatedCommandHelpers.h"
#include "Engine/AssetManager.h"
#include "Equipment/ArcEquipmentComponent.h"
#include "Equipment/ArcItemAttachmentComponent.h"
#include "GameFramework/PlayerState.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/ArcItemsStoreComponent.h"
#include "Items/Fragments/ArcItemFragment_AbilityEffectsToApply.h"
#include "Items/Fragments/ArcItemFragment_GrantedAbilities.h"
#include "Items/Fragments/ArcItemFragment_SocketSlots.h"
#include "Kismet/GameplayStatics.h"
#include "QuickBar/ArcQuickBarComponent.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "FArcGameplayDebuggerModule"

TMap<FName, TFunction<void(const FArcItemData*, const FArcItemInstance*)>> FArcGameplayDebuggerTools::ItemInstancesDraw = TMap<FName, TFunction<void(const FArcItemData*, const FArcItemInstance*)>>();

void FArcGameplayDebuggerModule::StartupModule()
{
	FArcGameplayDebuggerTools::ItemInstancesDraw.Add(FArcItemInstance_GrantedAbilities::StaticStruct()->GetFName(),
		&FArcItemInstanceDataDebugger::DrawGrantedAbilities);

	FArcGameplayDebuggerTools::ItemInstancesDraw.Add(FArcItemInstance_EffectToApply::StaticStruct()->GetFName(),
		&FArcItemInstanceDataDebugger::DrawAbilityEffectsToApply);
}

void FArcGameplayDebuggerModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

FArcGameplayDebuggerModule::FArcGameplayDebuggerModule()
	: DebuggerTools{TEXT("Arcx.GameplayDebugger.Tools"), TEXT("Arc Gameplay Debugger Tools")}
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

void FArcDebuggerWidgetMenu::Draw()
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
	
	SlateIM::Fill();
	SlateIM::HAlign(HAlign_Fill);
	SlateIM::VAlign(VAlign_Fill);
	const FTableRowStyle* TableRowStyle = &FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.AlternatingRow");
	
	SlateIM::BeginTabGroup("Debugger");
		SlateIM::BeginTabStack();
			SlateIM::BeginTab("ItemsDebuggerTabV1", FSlateIcon(), LOCTEXT("ItemsDebuggerTab", "Items"));
			{

				SlateIM::AutoSize();
				
				SlateIM::HAlign(HAlign_Fill);
				SlateIM::VAlign(VAlign_Fill);
		
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
											SlateIM::Text(FString::Printf(TEXT("%s : %s"), *GetNameSafe(ItemData->GetItemDefinition()), *ItemData->GetItemId().ToString()));
											
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
			}
			SlateIM::EndTab();
			SlateIM::BeginTab("QuickBarDebuggerTabV1", FSlateIcon(), LOCTEXT("QuickBarDebuggerTab", "Quick Bar"));
				if (QuickBarComponent)
				{
					const FArcSelectedQuickBarSlotList& ReplicatedQuickSlots = QuickBarComponent->GetReplicatedSelectedSlots();
					const TArray<FArcSelectedQuickBarSlot>& SelectedSlots = ReplicatedQuickSlots.GetItemArray();

					SlateIM::Fill();
					SlateIM::AutoSize();
	
					SlateIM::HAlign(HAlign_Fill);
					SlateIM::VAlign(VAlign_Fill);
					
					SlateIM::BeginVerticalStack();
					{
						SlateIM::Fill();
						SlateIM::AutoSize();
	
						SlateIM::HAlign(HAlign_Fill);
						SlateIM::VAlign(VAlign_Fill);
						SlateIM::BeginTable(nullptr, TableRowStyle);
	
						SlateIM::AddTableColumn(TEXT("BarId"));
						SlateIM::AddTableColumn(TEXT("QuickSlotId"));
						SlateIM::AddTableColumn(TEXT("AssignedItemId"));
						SlateIM::AddTableColumn(TEXT("OldAssignedItemId"));
						SlateIM::AddTableColumn(TEXT("IsSelected"));
						SlateIM::AddTableColumn(TEXT("FromServer"));
						SlateIM::AddTableColumn(TEXT("ClientSelected"));
						SlateIM::AddTableColumn(TEXT("ClientDeselected"));
						
						for (const FArcSelectedQuickBarSlot& SelectedSlot : SelectedSlots)
						{
							if (SlateIM::NextTableCell())
							{
								SlateIM::HAlign(HAlign_Fill);
								SlateIM::VAlign(VAlign_Center);
								SlateIM::Text(SelectedSlot.BarId.ToString());
							}
							if (SlateIM::NextTableCell())
							{
								SlateIM::HAlign(HAlign_Fill);
								SlateIM::VAlign(VAlign_Center);
								SlateIM::Text(SelectedSlot.QuickSlotId.ToString());
							}
							if (SlateIM::NextTableCell())
							{
								SlateIM::HAlign(HAlign_Fill);
								SlateIM::VAlign(VAlign_Center);
								SlateIM::Text(SelectedSlot.AssignedItemId.ToString());
							}
							if (SlateIM::NextTableCell())
							{
								SlateIM::HAlign(HAlign_Fill);
								SlateIM::VAlign(VAlign_Center);
								SlateIM::Text(TEXT(""));
							}
							if (SlateIM::NextTableCell())
							{
								SlateIM::HAlign(HAlign_Fill);
								SlateIM::VAlign(VAlign_Center);
								SlateIM::Text(TEXT(""));
							}
							if (SlateIM::NextTableCell())
							{
								SlateIM::HAlign(HAlign_Fill);
								SlateIM::VAlign(VAlign_Center);
								SlateIM::Text(TEXT(""));
							}
							if (SlateIM::NextTableCell())
							{
								SlateIM::HAlign(HAlign_Fill);
								SlateIM::VAlign(VAlign_Center);
								SlateIM::Text(TEXT(""));
							}
							if (SlateIM::NextTableCell())
							{
								SlateIM::HAlign(HAlign_Fill);
								SlateIM::VAlign(VAlign_Center);
								SlateIM::Text(TEXT(""));
							}
						}
	
						SlateIM::EndTable();
					}
					{
						
						SlateIM::AutoSize();
						SlateIM::HAlign(HAlign_Fill);
						SlateIM::VAlign(VAlign_Fill);
						SlateIM::MaxHeight(500.f);
						SlateIM::BeginTable(nullptr, TableRowStyle);

						SlateIM::AddTableColumn(TEXT("QuickBar"));
						const TArray<FArcQuickBar>& QuickBars =  QuickBarComponent->GetQuickBars();

						for (const FArcQuickBar& QuickBar : QuickBars)
						{
							struct Item
							{
								UArcItemsStoreComponent* ItemsStore = nullptr;
								FArcItemId ItemId;
								FString ItemName;
							};

							TArray<FString> ItemNames;
							TArray<Item> ItemsForQuickSlot;
								
							for (UArcItemsStoreComponent* ItemsStore : ItemsStores)
							{
								if (!ItemsStore)
								{
									continue;
								}

								TArray<const FArcItemData*> Items = ItemsStore->GetItems();
								for (const FArcItemData* ItemData : Items)
								{
									if (ItemData && ItemData->GetItemDefinition())
									{
										ItemNames.Add(ItemData->GetItemDefinition()->GetName());
										ItemsForQuickSlot.Add({ ItemsStore, ItemData->GetItemId(), ItemData->GetItemDefinition()->GetName()});
									}
								}
							}

							TArray<FPrimaryAssetId> FilteredItems;
							TArray<FString> FilteredItemNames;
							if (QuickBar.ItemRequiredTags.Num() > 0)
							{
								for (const FAssetData& ItemData : ItemDefinitions)
								{
									FGameplayTagContainer ItemTags = UArcItemDefinition::GetItemTags(ItemData);
									if (ItemTags.Num() > 0 && ItemTags.HasAll(QuickBar.ItemRequiredTags))
									{
										FilteredItems.Add(AssetManager->GetPrimaryAssetIdForData(ItemData));
										FilteredItemNames.Add(ItemData.AssetName.ToString());
									}
								}
							}
							
							if (SlateIM::NextTableCell())
							{
								SlateIM::HAlign(HAlign_Fill);
								SlateIM::VAlign(VAlign_Center);

								SlateIM::BeginHorizontalStack();
								SlateIM::Text(QuickBar.BarId.ToString());
								if (SlateIM::Button(TEXT("Cycle Backward")))
								{
									FGameplayTag SelectedQuickSlot = QuickBarComponent->GetFirstActiveSlot(QuickBar.BarId);
									auto Func = [](const FArcItemData* InItem) -> bool
									{
										return true;
									};
									QuickBarComponent->CycleSlotBackward(QuickBar.BarId, SelectedQuickSlot, Func);
								}
								if (SlateIM::Button(TEXT("Cycle Forward")))
								{
									FGameplayTag SelectedQuickSlot = QuickBarComponent->GetFirstActiveSlot(QuickBar.BarId);
									auto Func = [](const FArcItemData* InItem) -> bool
									{
										return true;
									};
									QuickBarComponent->CycleSlotForward(QuickBar.BarId, SelectedQuickSlot, Func);
								}
								SlateIM::EndHorizontalStack();
							}
							
							if (SlateIM::BeginTableRowChildren())
							{
								for (const FArcQuickBarSlot& QuickBarSlot : QuickBar.Slots)
								{
									FArcItemId ItemId = QuickBarComponent->GetItemId(QuickBar.BarId, QuickBarSlot.QuickBarSlotId);
									UArcItemsStoreComponent* ItemsStore = QuickBarComponent->GetItemStoreComponent(QuickBar.BarId);

									const FArcItemData* ItemData = ItemsStore ? ItemsStore->GetItemPtr(ItemId) : nullptr;
									if (SlateIM::NextTableCell())
									{
										SlateIM::HAlign(HAlign_Fill);
										SlateIM::VAlign(VAlign_Center);

										FString ItemName = ItemData ? GetNameSafe(ItemData->GetItemDefinition()) : TEXT("No Item");

										//bool bIsPedingActivation = QuickBarComponent->IsPendingActivation(QuickBar.BarId, QuickBarSlot.QuickBarSlotId);

										//ItemName += bIsPedingActivation ? TEXT(" (Pending Rep Activation)") : TEXT(" (Rep Active)");

										bool bIsQuickSlotActive = QuickBarComponent->IsQuickSlotActive(QuickBar.BarId, QuickBarSlot.QuickBarSlotId);

										ItemName += bIsQuickSlotActive ? TEXT(" (Active)") : TEXT(" (Inactive)");
										
										SlateIM::Text(FString::Printf(TEXT("%s : %s"), *QuickBarSlot.QuickBarSlotId.ToString(), *ItemName));
										if (SlateIM::BeginTableRowChildren())
										{
											if (SlateIM::NextTableCell())
											{
												SlateIM::Text(TEXT("Items:"));
											}
											if (SlateIM::BeginTableRowChildren())
											{
												SlateIM::BeginHorizontalStack();

												
												{
													int32 SelectedItemIndex = -1;
													for (int32 Idx = 0; Idx < ItemsForQuickSlot.Num(); ++Idx)
													{
														if (ItemId == ItemsForQuickSlot[Idx].ItemId)
														{
															SelectedItemIndex = Idx;
															break;
														}
													}
													
													SlateIM::HAlign(HAlign_Fill);
													SlateIM::VAlign(VAlign_Center);
													SlateIM::BeginVerticalStack();
													SlateIM::Text(TEXT("Items Store: "));
													
													SlateIM::AutoSize();
													SlateIM::MaxHeight(150.f);
													SlateIM::BeginScrollBox();
													if (SlateIM::SelectionList(ItemNames, SelectedItemIndex))
													{
														Arcx::SendServerCommand<FArcAddItemToQuickBarCommand>(World, QuickBarComponent, ItemsForQuickSlot[SelectedItemIndex].ItemsStore
															, ItemsForQuickSlot[SelectedItemIndex].ItemId, QuickBar.BarId, QuickBarSlot.QuickBarSlotId);
													}
													SlateIM::EndScrollBox();
													SlateIM::EndVerticalStack();
												}

												{
													int32 SelectedItemIndex = -1;
													if (ItemData)
													{
														for (int32 Idx = 0; Idx < FilteredItems.Num(); ++Idx)
														{
															if (ItemData->GetItemDefinitionId() == FilteredItems[Idx])
															{
																SelectedItemIndex = Idx;
																break;
															}
														}
													}
													SlateIM::HAlign(HAlign_Fill);
													SlateIM::VAlign(VAlign_Center);
													SlateIM::BeginVerticalStack();
													SlateIM::Text(TEXT("Items Defs: "));

													SlateIM::AutoSize();
													SlateIM::MaxHeight(150.f);
													SlateIM::BeginScrollBox();
													if (SlateIM::SelectionList(FilteredItemNames, SelectedItemIndex))
													{
														Arcx::SendServerCommand<FArcAddNewItemToQuickBarCommand>(World, QuickBarComponent, ItemsStore
															, FilteredItems[SelectedItemIndex], QuickBar.BarId, QuickBarSlot.QuickBarSlotId);
													}
													SlateIM::EndScrollBox();
													SlateIM::EndVerticalStack();
												}
												
												SlateIM::EndHorizontalStack();
											}
											SlateIM::EndTableRowChildren();
											
											if (SlateIM::NextTableCell())
											{
												SlateIM::Text(TEXT("Abilities: "));
											}
											if (SlateIM::BeginTableRowChildren())
											{
												if (ItemData)
												{
													const FArcItemInstance_GrantedAbilities* AbilityInstance = ArcItems::FindInstance<FArcItemInstance_GrantedAbilities>(ItemData);
													if (AbilityInstance)
													{
														for (const FGameplayAbilitySpecHandle& Spec : AbilityInstance->GetGrantedAbilities())
														{
															FGameplayAbilitySpec* AbilitySpec = AbilitySystem->FindAbilitySpecFromHandle(Spec);
															if (!AbilitySpec)
															{
																continue;
															}

															UArcCoreGameplayAbility* ArcAbility = Cast<UArcCoreGameplayAbility>(AbilitySpec->GetPrimaryInstance());
															if (!ArcAbility)
															{
																continue;
															}

															double StartTime = AbilitySystem->GetAbilityActivationStartTime(Spec);
															const float ActivationTime = ArcAbility->GetActivationTime();
															
															if (SlateIM::NextTableCell())
															{
																double CurrentTime = StartTime > -1 ? FPlatformTime::Seconds() - StartTime : 0.0;
																
																FString ActivationTimeStr = FString::Printf(TEXT("StartTime: %.2f, ActivationTime: %.2f CurrentTime %.2f"), StartTime, ActivationTime, CurrentTime);
																SlateIM::Text(ActivationTimeStr);
															}
														}
													}
												}
											}
											SlateIM::EndTableRowChildren();
										}
										SlateIM::EndTableRowChildren();
									}
								}
							}
							SlateIM::EndTableRowChildren();

							
						}
						SlateIM::EndTable();
					}
					SlateIM::EndVerticalStack();
				}
			SlateIM::EndTab();
			SlateIM::BeginTab("ItemAttachmentDebuggerTabV1", FSlateIcon(), LOCTEXT("ItemAttachmentDebugger", "ItemAttachment"));
			if (ItemAttachmentComponent)
			{
				const FArcItemAttachmentSlotContainer& StaticSlotConfig = ItemAttachmentComponent->GetStaticAttachmentSlots();

				SlateIM::Fill();
				SlateIM::AutoSize();
	
				SlateIM::HAlign(HAlign_Fill);
				SlateIM::VAlign(VAlign_Fill);
				SlateIM::BeginTable();
				SlateIM::AddTableColumn(TEXT("Item"));
				for (const FArcItemAttachmentSlot& AttachSlot : StaticSlotConfig.Slots)
				{
					if (SlateIM::NextTableCell())
					{
						SlateIM::Text(AttachSlot.SlotId.ToString());
					}
					if (SlateIM::BeginTableRowChildren())
					{
						FAssetData DefaultItem;
						AssetManager->GetPrimaryAssetData(AttachSlot.DefaultVisualItem, DefaultItem);
						SlateIM::BeginVerticalStack();
						SlateIM::Text(FString::Printf(TEXT("Default Visual Item: %s"), *DefaultItem.AssetName.ToString()));
						SlateIM::EndVerticalStack();
					}
					SlateIM::EndTableRowChildren();
				}
				SlateIM::EndTable();

				const FArcItemAttachmentContainer& ReplicatedAttachments = ItemAttachmentComponent->GetReplicatedAttachments();
				//UClass* ItemStoreClass = ItemAttachmentComponent->GetItemStoreClass();
				//UArcItemsStoreComponent* AttachmentStore = Arcx::Utils::GetComponent<UArcItemsStoreComponent>(PS, ItemStoreClass);
				
				SlateIM::HAlign(HAlign_Fill);
				SlateIM::VAlign(VAlign_Fill);
				SlateIM::BeginTable();
				SlateIM::AddTableColumn(TEXT("ReplicatedAttachments"));
				for (const FArcItemAttachment& Attachment : ReplicatedAttachments.Items)
				{
					if (SlateIM::NextTableCell())
					{
						SlateIM::Text(Attachment.SlotId.ToString());
					}
					if (SlateIM::BeginTableRowChildren())
					{
						SlateIM::BeginVerticalStack();
						SlateIM::Text(FString::Printf(TEXT("ItemId: %s"), *Attachment.ItemId.ToString()));
						SlateIM::Text(FString::Printf(TEXT("OwnerId: %s"), *Attachment.OwnerId.ToString()));
						SlateIM::Text(FString::Printf(TEXT("SocketName: %s"), *Attachment.SocketName.ToString()));
						SlateIM::Text(FString::Printf(TEXT("SocketComponentTag: %s"), *Attachment.SocketComponentTag.ToString()));
						SlateIM::Text(FString::Printf(TEXT("ChangedSocket: %s"), *Attachment.ChangedSocket.ToString()));
						SlateIM::Text(FString::Printf(TEXT("ChangeSceneComponentTag: %s"), *Attachment.ChangeSceneComponentTag.ToString()));
						SlateIM::Text(FString::Printf(TEXT("OwnerSlotId: %s"), *Attachment.OwnerSlotId.ToString()));
						SlateIM::Text(FString::Printf(TEXT("RelativeTransform: %s"), *Attachment.RelativeTransform.ToString()));
						SlateIM::Text(FString::Printf(TEXT("VisualItemDefinition: %s"), *GetNameSafe(Attachment.VisualItemDefinition)));
						SlateIM::Text(FString::Printf(TEXT("OldVisualItemDefinition: %s"), *GetNameSafe(Attachment.OldVisualItemDefinition)));
						SlateIM::Text(FString::Printf(TEXT("ItemDefinition: %s"), *GetNameSafe(Attachment.ItemDefinition)));
						SlateIM::Text(FString::Printf(TEXT("OwnerItemDefinition: %s"), *GetNameSafe(Attachment.OwnerItemDefinition)));
						
						SlateIM::EndVerticalStack();
					}
					SlateIM::EndTableRowChildren();
				}
				SlateIM::EndTable();
			}
			SlateIM::EndTab();
		SlateIM::EndTabStack();
	
	SlateIM::EndTabGroup();
}

void FArcGameplayDebuggerTools::DrawWindow(float DeltaTime)
{
	TestWidget.Draw();
	
//	if (GEngine && GEngine->GameViewport)
//	{
//		if (SlateIM::BeginViewportRoot("ArcDebuggerMenu", GEngine->GameViewport, Layout))
//		{
//			TestWidget.Draw();
//		}
//		SlateIM::EndRoot();
//	}
//#if WITH_EDITOR
//	else if (GCurrentLevelEditingViewportClient)
//	{
//		TSharedPtr<SLevelViewport> LevelViewport = StaticCastSharedPtr<SLevelViewport>(GCurrentLevelEditingViewportClient->GetEditorViewportWidget());
//		if (LevelViewport.IsValid())
//		{
//			if (SlateIM::BeginViewportRoot("ArcDebuggerMenu", LevelViewport, Layout))
//			{
//				TestWidget.Draw();
//			}
//			SlateIM::EndRoot();
//		}
//	}
//#endif
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FArcGameplayDebuggerModule, ArcGameplayDebugger)