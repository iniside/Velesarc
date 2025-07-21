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
#include "Items/Fragments/ArcItemFragment_ItemStats.h"
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


void FArcDebuggerItemStatsEditorWidget::Draw(FArcItemFragment_ItemStats* InStats)
{
	TArray<FString> Attributes;
	TArray<FGameplayAttribute> GameplayAttributes;
	for (TObjectIterator<UAttributeSet> It; It; ++It)
	{
		for (TFieldIterator<FStructProperty> PropIt(It->GetClass()); PropIt; ++PropIt)
		{
			if (PropIt->Struct->IsChildOf(FGameplayAttributeData::StaticStruct()))
			{
				Attributes.Add(FString::Printf(TEXT("%s.%s"), *It->GetClass()->GetName(), *PropIt->GetName()));
				GameplayAttributes.Add(FGameplayAttribute(*PropIt));
			}
		}
	}
	
	if (InStats)
	{
		const TArray<FArcItemAttributeStat>& DefaultStats = InStats->DefaultStats;
		
		if (SlateIM::NextTableCell())
		{
			SlateIM::SpinBox(StatValue, 0.f, 100.f);

			SlateIM::ComboBox(Attributes, SelectedAttributeIdx);
			if (SlateIM::Button(TEXT("Add")))
			{
				if (SelectedAttributeIdx != INDEX_NONE)
				{
					FArcItemAttributeStat NewStat;
					NewStat.Attribute = GameplayAttributes[SelectedAttributeIdx];
					NewStat.Value = StatValue;
					InStats->DefaultStats.Add(NewStat);
				}
			}
		}
		
		if (SlateIM::NextTableCell())
		{
			SlateIM::Text(TEXT("Default Stats"));
		}
		if (SlateIM::BeginTableRowChildren())
		{
			for (const FArcItemAttributeStat& Stat : DefaultStats)
			{
				if (SlateIM::NextTableCell())
				{
					SlateIM::Text(Stat.Attribute.AttributeName);
				}
			}
		}
		SlateIM::EndTableRowChildren();
	}
}

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
				ItemsDebugger.Draw();
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

			SlateIM::BeginTab("ItemMakerTabV1", FSlateIcon(), LOCTEXT("ItemMakerDebugger", "Item Maker"));
			{
				ItemSpecEditor.Draw();
			}			
			SlateIM::EndTab();
		SlateIM::EndTabStack();
	
	SlateIM::EndTabGroup();
}

void FArcDebuggerWidgetMenu::Initialize()
{
	ItemsDebugger.Initialize();
	ItemSpecEditor.Initialize();
}

void FArcDebuggerWidgetMenu::Uninitialize()
{
	ItemsDebugger.Uninitialize();
	ItemSpecEditor.Uninitialize();
}

void FArcSlateIMWindowBase::DrawWidget(float DeltaTime)
{
	PreDraw(DeltaTime);
	if (!SlateIM::CanUpdateSlateIM())
	{
		return;
	}
	
	const bool bIsDrawingWindow = SlateIM::BeginWindowRoot(GetWidgetName(), WindowTitle, WindowSize);
	if (bIsDrawingWindow)
	{
		DrawWindow(DeltaTime);
	}
	SlateIM::EndRoot();

	if (!bIsDrawingWindow)
	{
		DisableWidget();
	}
	PostDraw(DeltaTime);
}

void FArcGameplayDebuggerTools::PreDraw(float DeltaTime)
{
	if (IsWidgetEnabled())
	{
		if (WidgetState != EWidgetState::Enabled)
		{
			UE_LOG(LogTemp, Log, TEXT("ArcGameplayDebuggerTools: Widget Enabled"));
			WidgetState = EWidgetState::Enabled;
			Initialize();
		}
	}
}

void FArcGameplayDebuggerTools::DrawWindow(float DeltaTime)
{
	TestWidget.Draw();
}

void FArcGameplayDebuggerTools::PostDraw(float DeltaTime)
{
	if (!IsWidgetEnabled())
	{
		if (WidgetState != EWidgetState::Disabled)
		{
			UE_LOG(LogTemp, Log, TEXT("ArcGameplayDebuggerTools: Widget Disabled"));
			WidgetState = EWidgetState::Disabled;
			Uninitialize();
		}
	}
}

void FArcGameplayDebuggerTools::Initialize()
{
	TestWidget.Initialize();
}

void FArcGameplayDebuggerTools::Uninitialize()
{
	TestWidget.Uninitialize();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FArcGameplayDebuggerModule, ArcGameplayDebugger)