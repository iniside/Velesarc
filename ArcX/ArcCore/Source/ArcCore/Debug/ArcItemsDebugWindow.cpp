// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcItemsDebugWindow.h"

#include "SlateIM.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemsStoreComponent.h"
#include "Items/ArcItemSpec.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/AppStyle.h"

;

FArcItemsDebugWindow::FArcItemsDebugWindow()
	: FSlateIMWindowBase(
		TEXT("Arc Items Debug"),
		FVector2f(800.f, 600.f),
		TEXT("Arc.Items.Debug"),
		TEXT("Toggle the Arc Items debug window"))
{
}

TArray<UArcItemsStoreComponent*> FArcItemsDebugWindow::GetLocalPlayerStores() const
{
	TArray<UArcItemsStoreComponent*> Result;

	if (!World.IsValid())
	{
		World = GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
		if (!World.IsValid())
		{
			return Result;	
		}
		
	}
	
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return Result;
	}

	APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		return Result;
	}

	TArray<AActor*> ActorsToSearch;
	ActorsToSearch.Add(Pawn);
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

void FArcItemsDebugWindow::RefreshItemDefinitions()
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

void FArcItemsDebugWindow::DrawWindow(float DeltaTime)
{
	SlateIM::BeginBorder(FAppStyle::GetBrush("ToolPanel.GroupBorder"));
	SlateIM::Fill();
	SlateIM::HAlign(HAlign_Fill);
	SlateIM::VAlign(VAlign_Fill);
	SlateIM::BeginVerticalStack();
	{
		DrawStoreSelector();

		if (SelectedStore.IsValid())
		{
			SlateIM::Padding(FMargin(4.f));
			SlateIM::Fill();
			SlateIM::BeginHorizontalStack();
			{
				// Left panel: items list
				SlateIM::Fill();
				SlateIM::HAlign(HAlign_Fill);
				SlateIM::VAlign(VAlign_Fill);
				SlateIM::BeginVerticalStack();
				{
					DrawItemsList();
				}
				SlateIM::EndVerticalStack();

				// Right panel: item detail
				SlateIM::Fill();
				SlateIM::HAlign(HAlign_Fill);
				SlateIM::VAlign(VAlign_Fill);
				SlateIM::BeginVerticalStack();
				{
					TArray<const FArcItemData*> Items = SelectedStore->GetItems();
					if (SelectedItemIndex >= 0 && SelectedItemIndex < Items.Num())
					{
						DrawItemDetail(Items[SelectedItemIndex]);
					}
					else
					{
						SlateIM::Padding(FMargin(8.f));
						SlateIM::Text(TEXT("Select an item to view details"));
					}
				}
				SlateIM::EndVerticalStack();
			}
			SlateIM::EndHorizontalStack();
		}
		else
		{
			SlateIM::Padding(FMargin(8.f));
			SlateIM::Text(TEXT("No ItemsStoreComponent selected"));
		}
	}
	SlateIM::EndVerticalStack();
}

void FArcItemsDebugWindow::DrawStoreSelector()
{
	TArray<UArcItemsStoreComponent*> Stores = GetLocalPlayerStores();

	SlateIM::Padding(FMargin(4.f));
	SlateIM::BeginHorizontalStack();
	{
		SlateIM::Text(TEXT("Store: "));

		if (Stores.Num() == 0)
		{
			SlateIM::Text(TEXT("No stores found on local player"));
		}
		else
		{
			TArray<FString> StoreNames;
			for (UArcItemsStoreComponent* Store : Stores)
			{
				FString Name = FString::Printf(TEXT("%s (%s) [%d items]"),
					*Store->GetName(),
					*GetNameSafe(Store->GetOwner()),
					Store->GetItemNum());
				StoreNames.Add(Name);
			}

			bool bForceRefresh = false;
			if (!SelectedStore.IsValid() && Stores.Num() > 0)
			{
				SelectedStoreIndex = 0;
				SelectedStore = Stores[0];
				bForceRefresh = true;
			}

			SlateIM::MinWidth(300.f);
			if (SlateIM::ComboBox(StoreNames, SelectedStoreIndex, bForceRefresh))
			{
				if (SelectedStoreIndex >= 0 && SelectedStoreIndex < Stores.Num())
				{
					SelectedStore = Stores[SelectedStoreIndex];
					SelectedItemIndex = -1;
				}
			}
		}
	}
	SlateIM::EndHorizontalStack();
}

void FArcItemsDebugWindow::DrawItemsList()
{
	if (!SelectedStore.IsValid())
	{
		return;
	}

	SlateIM::Padding(FMargin(4.f));
	SlateIM::BeginHorizontalStack();
	{
		SlateIM::Text(TEXT("Items"));
		SlateIM::Padding(FMargin(4.f, 0.f));
		if (SlateIM::Button(TEXT("Add Item")))
		{
			bShowAddItemPanel = !bShowAddItemPanel;
			if (bShowAddItemPanel && CachedItemDefinitions.Num() == 0)
			{
				RefreshItemDefinitions();
				FilteredDefinitionNames = CachedItemDefinitionNames;
				FilteredToSourceIndex.Reset();
				for (int32 i = 0; i < CachedItemDefinitionNames.Num(); ++i)
				{
					FilteredToSourceIndex.Add(i);
				}
			}
		}
	}
	SlateIM::EndHorizontalStack();

	if (bShowAddItemPanel)
	{
		DrawAddItemPanel();
	}

	// Items table
	SlateIM::Padding(FMargin(4.f));
	SlateIM::Fill();
	SlateIM::FTableParams TableParams;
	TableParams.SelectionMode = ESelectionMode::Single;
	SlateIM::BeginTable(TableParams);
	{
		SlateIM::BeginTableHeader();
		{
			SlateIM::InitialTableColumnWidth(180.f);
			SlateIM::AddTableColumn(TEXT("Definition"), TEXT("Definition"));
			SlateIM::InitialTableColumnWidth(60.f);
			SlateIM::AddTableColumn(TEXT("Stacks"), TEXT("Stacks"));
			SlateIM::InitialTableColumnWidth(60.f);
			SlateIM::AddTableColumn(TEXT("Level"), TEXT("Level"));
			SlateIM::InitialTableColumnWidth(120.f);
			SlateIM::AddTableColumn(TEXT("Slot"), TEXT("Slot"));
			SlateIM::FixedTableColumnWidth(70.f);
			SlateIM::AddTableColumn(TEXT("Actions"), TEXT("Actions"));
		}
		SlateIM::EndTableHeader();

		SlateIM::BeginTableBody();
		{
			TArray<const FArcItemData*> Items = SelectedStore->GetItems();
			for (int32 i = 0; i < Items.Num(); ++i)
			{
				const FArcItemData* Item = Items[i];
				if (!Item)
				{
					continue;
				}

				bool bRowSelected = false;

				// Definition column
				if (SlateIM::NextTableCell(&bRowSelected))
				{
					const UArcItemDefinition* Def = Item->GetItemDefinition();
					SlateIM::Text(Def ? GetNameSafe(Def) : TEXT("null"));
				}

				if (bRowSelected)
				{
					SelectedItemIndex = i;
				}

				// Stacks column
				if (SlateIM::NextTableCell())
				{
					SlateIM::Text(*FString::Printf(TEXT("%d"), Item->GetStacks()));
				}

				// Level column
				if (SlateIM::NextTableCell())
				{
					SlateIM::Text(*FString::Printf(TEXT("%d"), Item->GetLevel()));
				}

				// Slot column
				if (SlateIM::NextTableCell())
				{
					FString SlotStr = Item->Slot.IsValid() ? Item->Slot.ToString() : TEXT("-");
					SlateIM::Text(*SlotStr);
				}

				// Actions column
				if (SlateIM::NextTableCell())
				{
					if (SlateIM::Button(TEXT("Remove")))
					{
						SelectedStore->RemoveItem(Item->GetItemId(), Item->GetStacks());
						if (SelectedItemIndex >= Items.Num() - 1)
						{
							SelectedItemIndex = Items.Num() - 2;
						}
					}
				}
			}
		}
		SlateIM::EndTableBody();
	}
	SlateIM::EndTable();
}

void FArcItemsDebugWindow::DrawAddItemPanel()
{
	SlateIM::Padding(FMargin(4.f));
	SlateIM::BeginBorder(TEXT("ToolPanel.GroupBorder"));
	{
		SlateIM::Text(TEXT("Add New Item"));

		SlateIM::Padding(FMargin(2.f));
		SlateIM::BeginHorizontalStack();
		{
			SlateIM::Text(TEXT("Filter: "));
			SlateIM::MinWidth(200.f);
			if (SlateIM::EditableText(FilterText, TEXT("Search definitions...")))
			{
				FilteredDefinitionNames.Reset();
				FilteredToSourceIndex.Reset();
				FString Lower = FilterText.ToLower();
				for (int32 i = 0; i < CachedItemDefinitionNames.Num(); ++i)
				{
					if (FilterText.IsEmpty() || CachedItemDefinitionNames[i].ToLower().Contains(Lower))
					{
						FilteredDefinitionNames.Add(CachedItemDefinitionNames[i]);
						FilteredToSourceIndex.Add(i);
					}
				}
				SelectedDefinitionIndex = 0;
			}

			SlateIM::Padding(FMargin(4.f, 0.f));
			if (SlateIM::Button(TEXT("Refresh")))
			{
				RefreshItemDefinitions();
				FilteredDefinitionNames = CachedItemDefinitionNames;
				FilteredToSourceIndex.Reset();
				for (int32 i = 0; i < CachedItemDefinitionNames.Num(); ++i)
				{
					FilteredToSourceIndex.Add(i);
				}
				FilterText.Empty();
			}
		}
		SlateIM::EndHorizontalStack();

		if (FilteredDefinitionNames.Num() > 0)
		{
			SlateIM::Padding(FMargin(2.f));
			SlateIM::MaxHeight(150.f);
			SlateIM::SelectionList(FilteredDefinitionNames, SelectedDefinitionIndex);

			SlateIM::Padding(FMargin(2.f));
			if (SlateIM::Button(TEXT("Add Selected")))
			{
				if (SelectedDefinitionIndex >= 0 && SelectedDefinitionIndex < FilteredToSourceIndex.Num())
				{
					int32 SourceIdx = FilteredToSourceIndex[SelectedDefinitionIndex];
					if (SourceIdx >= 0 && SourceIdx < CachedItemDefinitions.Num())
					{
						UArcItemDefinition* Def = Cast<UArcItemDefinition>(CachedItemDefinitions[SourceIdx].GetAsset());
						if (Def && SelectedStore.IsValid())
						{
							FArcItemSpec Spec = FArcItemSpec::NewItem(Def, 1, 1);
							SelectedStore->AddItem(Spec, FArcItemId());
						}
					}
				}
			}
		}
		else
		{
			SlateIM::Padding(FMargin(2.f));
			SlateIM::Text(TEXT("No definitions found"));
		}

		SlateIM::Padding(FMargin(2.f));
		if (SlateIM::Button(TEXT("Close")))
		{
			bShowAddItemPanel = false;
		}
	}
	SlateIM::EndBorder();
}

void FArcItemsDebugWindow::DrawItemDetail(const FArcItemData* Item)
{
	if (!Item)
	{
		return;
	}

	SlateIM::Padding(FMargin(4.f));
	SlateIM::BeginScrollBox();
	{
		const UArcItemDefinition* Def = Item->GetItemDefinition();

		// Basic info
		SlateIM::Text(*FString::Printf(TEXT("Definition: %s"), *GetNameSafe(Def)));
		SlateIM::Text(*FString::Printf(TEXT("ItemId: %s"), *Item->GetItemId().ToString()));
		SlateIM::Text(*FString::Printf(TEXT("Level: %d"), Item->GetLevel()));
		SlateIM::Text(*FString::Printf(TEXT("Stacks: %d"), Item->GetStacks()));

		FString SlotStr = Item->Slot.IsValid() ? Item->Slot.ToString() : TEXT("None");
		SlateIM::Text(*FString::Printf(TEXT("Slot: %s"), *SlotStr));

		if (Item->OwnerId.IsValid())
		{
			SlateIM::Text(*FString::Printf(TEXT("Owner: %s"), *Item->OwnerId.ToString()));
		}

		// Slot controls
		SlateIM::Spacer(FVector2D(0.0, 8.0));
		SlateIM::Text(TEXT("--- Slot Controls ---"));
		SlateIM::Padding(FMargin(2.f));
		SlateIM::BeginHorizontalStack();
		{
			SlateIM::MinWidth(200.f);
			SlateIM::EditableText(SlotTagText, TEXT("Gameplay tag (e.g. Slot.Primary)"));
			SlateIM::Padding(FMargin(4.f, 0.f));
			if (SlateIM::Button(TEXT("Assign Slot")))
			{
				if (!SlotTagText.IsEmpty() && SelectedStore.IsValid())
				{
					FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*SlotTagText), false);
					if (Tag.IsValid())
					{
						SelectedStore->AddItemToSlot(Item->GetItemId(), Tag);
					}
				}
			}
			SlateIM::Padding(FMargin(4.f, 0.f));
			if (Item->Slot.IsValid())
			{
				if (SlateIM::Button(TEXT("Remove from Slot")))
				{
					if (SelectedStore.IsValid())
					{
						SelectedStore->RemoveItemFromSlot(Item->GetItemId());
					}
				}
			}
		}
		SlateIM::EndHorizontalStack();

		// Attached items
		if (Item->AttachedItems.Num() > 0)
		{
			SlateIM::Spacer(FVector2D(0.0, 8.0));
			SlateIM::Text(TEXT("--- Attached Items ---"));
			for (const FArcItemId& AttId : Item->AttachedItems)
			{
				const FArcItemData* Att = SelectedStore->GetItemPtr(AttId);
				if (Att)
				{
					FString AttSlot = Att->AttachedToSlot.IsValid() ? Att->AttachedToSlot.ToString() : TEXT("-");
					SlateIM::Text(*FString::Printf(TEXT("  [%s] %s (Slot: %s)"),
						*AttId.ToString(),
						*GetNameSafe(Att->GetItemDefinition()),
						*AttSlot));
				}
			}
		}

		// Dynamic Tags
		if (Item->DynamicTags.Num() > 0)
		{
			SlateIM::Spacer(FVector2D(0.0, 8.0));
			SlateIM::Text(TEXT("--- Dynamic Tags ---"));
			TArray<FGameplayTag> Tags;
			Item->DynamicTags.GetGameplayTagArray(Tags);
			for (const FGameplayTag& Tag : Tags)
			{
				SlateIM::Text(*FString::Printf(TEXT("  %s"), *Tag.ToString()));
			}
		}

		// Aggregated Tags
		if (Item->ItemAggregatedTags.Num() > 0)
		{
			SlateIM::Spacer(FVector2D(0.0, 8.0));
			SlateIM::Text(TEXT("--- Aggregated Tags ---"));
			TArray<FGameplayTag> Tags;
			Item->ItemAggregatedTags.GetGameplayTagArray(Tags);
			for (const FGameplayTag& Tag : Tags)
			{
				SlateIM::Text(*FString::Printf(TEXT("  %s"), *Tag.ToString()));
			}
		}

		// Definition fragments
		if (Def)
		{
			SlateIM::Spacer(FVector2D(0.0, 8.0));
			SlateIM::Text(TEXT("--- Definition Fragments ---"));

			const TSet<FArcInstancedStruct>& Fragments = Def->GetFragmentSet();
			for (const FArcInstancedStruct& IS : Fragments)
			{
				if (!IS.IsValid())
				{
					continue;
				}

				const UScriptStruct* Struct = IS.GetScriptStruct();
				SlateIM::Text(*FString::Printf(TEXT("  %s"), *GetNameSafe(Struct)));
			}

			const TSet<FArcInstancedStruct>& ScalableFragments = Def->GetScalableFloatFragments();
			if (ScalableFragments.Num() > 0)
			{
				SlateIM::Spacer(FVector2D(0.0, 4.0));
				SlateIM::Text(TEXT("  (Scalable Float Fragments)"));
				for (const FArcInstancedStruct& IS : ScalableFragments)
				{
					if (!IS.IsValid())
					{
						continue;
					}
					SlateIM::Text(*FString::Printf(TEXT("    %s"), *GetNameSafe(IS.GetScriptStruct())));
				}
			}
		}

		// Instanced data tree
		SlateIM::Spacer(FVector2D(0.0, 8.0));
		DrawInstancedDataTree(Item);
	}
	SlateIM::EndScrollBox();
}

void FArcItemsDebugWindow::DrawInstancedDataTree(const FArcItemData* Item)
{
	if (!Item)
	{
		return;
	}

	SlateIM::Text(TEXT("--- Instanced Data ---"));

	if (Item->InstancedData.Num() == 0 && Item->ItemInstances.Data.Num() == 0)
	{
		SlateIM::Padding(FMargin(8.f, 2.f));
		SlateIM::Text(TEXT("(none)"));
		return;
	}

	// InstancedData map (TMap<FName, TSharedPtr<FArcItemInstance>>)
	SlateIM::BeginTable();
	{
		SlateIM::BeginTableHeader();
		{
			SlateIM::InitialTableColumnWidth(200.f);
			SlateIM::AddTableColumn(TEXT("Type"), TEXT("Type"));
			SlateIM::InitialTableColumnWidth(300.f);
			SlateIM::AddTableColumn(TEXT("Data"), TEXT("Data"));
		}
		SlateIM::EndTableHeader();

		SlateIM::BeginTableBody();
		{
			uint32 RowId = 1;

			for (const auto& Pair : Item->InstancedData)
			{
				// Type name cell
				if (SlateIM::NextTableCell())
				{
					FString TypeName = Pair.Key.ToString();
					if (Pair.Value.IsValid())
					{
						TypeName = GetNameSafe(Pair.Value->GetScriptStruct());
					}
					SlateIM::Text(*TypeName);
				}

				// Data cell — expand properties using UScriptStruct reflection
				if (SlateIM::NextTableCell())
				{
					if (Pair.Value.IsValid())
					{
						SlateIM::Text(*Pair.Value->ToString());
					}
					else
					{
						SlateIM::Text(TEXT("(null)"));
					}
				}

				// Tree children: struct properties
				if (Pair.Value.IsValid())
				{
					if (SlateIM::BeginTableRowChildren(RowId, false))
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

								if (SlateIM::NextTableCell())
								{
									SlateIM::Text(*Prop->GetName());
								}
								if (SlateIM::NextTableCell())
								{
									SlateIM::Text(*ValueStr);
								}
							}
						}
					}
					SlateIM::EndTableRowChildren();
				}

				++RowId;
			}

			// Also show replicated ItemInstances
			for (int32 Idx = 0; Idx < Item->ItemInstances.Data.Num(); ++Idx)
			{
				const FArcItemInstanceInternal& Instance = Item->ItemInstances.Data[Idx];
				if (!Instance.IsValid())
				{
					continue;
				}

				if (SlateIM::NextTableCell())
				{
					SlateIM::Text(*FString::Printf(TEXT("[Rep] %s"), *GetNameSafe(Instance.Data->GetScriptStruct())));
				}

				if (SlateIM::NextTableCell())
				{
					SlateIM::Text(*Instance.Data->ToString());
				}

				if (SlateIM::BeginTableRowChildren(RowId, false))
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

							if (SlateIM::NextTableCell())
							{
								SlateIM::Text(*Prop->GetName());
							}
							if (SlateIM::NextTableCell())
							{
								SlateIM::Text(*ValueStr);
							}
						}
					}
				}
				SlateIM::EndTableRowChildren();

				++RowId;
			}
		}
		SlateIM::EndTableBody();
	}
	SlateIM::EndTable();
}
