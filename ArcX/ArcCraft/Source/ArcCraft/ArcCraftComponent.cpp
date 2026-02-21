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

#include "ArcCraftComponent.h"

#include "ArcCoreUtils.h"
#include "ArcCraftExecution_Recipe.h"
#include "Core/ArcCoreAssetManager.h"
#include "Items/ArcItemsStoreComponent.h"
#include "Items/Factory/ArcItemSpecGeneratorDefinition.h"
#include "Items/Fragments/ArcItemFragment_RequiredItems.h"
#include "Net/UnrealNetwork.h"
#include "Recipe/ArcRecipeDefinition.h"

bool FArcCraftRequirement_InstigatorItemsStore::CheckAndConsumeItems(const UArcCraftComponent* InCraftComponent, const UArcItemDefinition* InCraftData
	, const UObject* InInstigator, bool bConsume) const
{
	const FArcItemFragment_RequiredItems* RequiredItems = InCraftData->FindFragment<FArcItemFragment_RequiredItems>();
	if (!RequiredItems)
	{
		return true;
	}
	const AActor* OwnerActor = Cast<AActor>(InInstigator);
	if (!OwnerActor)
	{
		return false;
	}
	
	UArcItemsStoreComponent* ItemsStore = Arcx::Utils::GetComponent(OwnerActor, ItemsStoreClass);
	if (!ItemsStore)
	{
		return false;
	}

	TArray<TPair<FArcItemId, int32>> ItemDefsNeeded;
	
	// First check if we have anough items to begin with.
	for (const FArcItemDefCount& RequiredItem : RequiredItems->RequiredItemDefs)
	{
		const FArcItemData* ItemData =  ItemsStore->GetItemByDefinition(RequiredItem.ItemDefinitionId);
		if (!ItemData)
		{
			return false;
		}

		if (ItemData->GetStacks() < RequiredItem.Count)
		{
			return false;
		}
		
		ItemDefsNeeded.Add({ItemData->GetItemId(), RequiredItem.Count});
	}

	for (const FArcItemTagCount& RequiredItem : RequiredItems->RequiredItemsWithTags)
	{
		const FArcItemData* ItemData =  ItemsStore->GetItemByTags(RequiredItem.RequiredTags);
		if (!ItemData)
		{
			return false;
		}

		if (ItemData->GetStacks() < RequiredItem.Count)
		{
			return false;
		}
		
		ItemDefsNeeded.Add({ItemData->GetItemId(), RequiredItem.Count});
	}

	if (bConsume)
	{
		for (const TPair<FArcItemId, int32>& ItemDef : ItemDefsNeeded)
		{
			ItemsStore->RemoveItem(ItemDef.Key, ItemDef.Value, true);
		}	
	}
	
	return true;
}

TArray<FArcCraftItemAmount> FArcCraftRequirement_InstigatorItemsStore::AvailableItems(const UArcCraftComponent* InCraftComponent, const UArcItemDefinition* InCraftData, const UObject* InInstigator) const
{
	TArray<FArcCraftItemAmount> AvailableItems;

	const FArcItemFragment_RequiredItems* RequiredItems = InCraftData->FindFragment<FArcItemFragment_RequiredItems>();
	if (!RequiredItems)
	{
		return AvailableItems;
	}
	const AActor* OwnerActor = Cast<AActor>(InInstigator);
	if (!OwnerActor)
	{
		return AvailableItems;
	}
	
	UArcItemsStoreComponent* ItemsStore = Arcx::Utils::GetComponent(OwnerActor, ItemsStoreClass);
	if (!ItemsStore)
	{
		return AvailableItems;
	}

	for (const FArcItemDefCount& RequiredItem : RequiredItems->RequiredItemDefs)
	{
		int32 Amount = ItemsStore->CountItemsByDefinition(RequiredItem.ItemDefinitionId);
		AvailableItems.Add({RequiredItem.ItemDefinitionId, Amount});
	}
	
	return AvailableItems;
}

FArcItemSpec FArcCraftRequirement_InstigatorItemsStore::OnCraftFinished(UArcCraftComponent* InCraftComponent, const UArcItemDefinition* InCraftData, const UObject* InInstigator) const
{
	const FArcItemFragment_CraftData* CraftData = InCraftData->FindFragment<FArcItemFragment_CraftData>();
	FArcItemSpec NewItemSpec;
	if (!CraftData)
	{
		return NewItemSpec;
	}

	const AActor* OwnerActor = Cast<AActor>(InInstigator);
	if (!OwnerActor)
	{
		return NewItemSpec;
	}

	UArcItemsStoreComponent* ItemsStore = Arcx::Utils::GetComponent(OwnerActor, ItemsStoreClass);
	if (!ItemsStore)
	{
		return NewItemSpec;
	}

	const FArcItemFragment_ItemGenerator* ItemGenerator = InCraftData->FindFragment<FArcItemFragment_ItemGenerator>();
	if (ItemGenerator && ItemGenerator->ItemGeneratorDef)
	{
		
	}
	else
	{
		NewItemSpec = FArcItemSpec::NewItem(CraftData->ItemDefinition, 1, 1);

		ItemsStore->AddItem(NewItemSpec, FArcItemId::InvalidId);
	}

	
	return NewItemSpec;
}

// Sets default values for this component's properties
UArcCraftComponent::UArcCraftComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	PrimaryComponentTick.TickInterval = 0.5f;
	
	SetIsReplicatedByDefault(true);
	// ...
}

// Called when the game starts
void UArcCraftComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

void UArcCraftComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UArcCraftComponent, CraftedItemList, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UArcCraftComponent, StoredResourceItemList, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UArcCraftComponent, CraftedItemSpecList, COND_OwnerOnly);
}

void UArcCraftComponent::UpdateStatus()
{
	const FDateTime Time = FDateTime::UtcNow();
	const int64 CurrentTime = Time.ToUnixTimestamp();
	
	for (FArcCraftItem& Item : CraftedItemList.Items)
	{
		const FArcItemFragment_CraftData* CraftData = Item.Recipe->FindFragment<FArcItemFragment_CraftData>();
		
		const int32 OneItemCraftTime = CraftData->CraftTime;
		const int64 ElapsedTime = CurrentTime - Item.StartTime;

		// How much items cloud we created in the time that elapsed since we last checked ?
		int32 PossiblyCraftedItems = ElapsedTime / OneItemCraftTime;
	}
}

bool UArcCraftComponent::CheckRequirements(const UArcItemDefinition* InCraftData, const UObject* Instigator) const
{
	const FArcItemFragment_CraftData* CraftData = InCraftData->FindFragment<FArcItemFragment_CraftData>();
	if (!CraftData)
	{
		return false;
	}
	
	if (!CraftExecution.GetPtr<FArcCraftExecution>()->CheckAndConsumeItems(this, InCraftData, Instigator, false))
	{
		UE_LOG(LogTemp, Warning, TEXT("Crafting requirements not met for %s"), *InCraftData->GetName());
		return false;
	}
	

	return true;
}

bool UArcCraftComponent::CheckRecipeRequirements(const UArcRecipeDefinition* InRecipe, const UObject* Instigator) const
{
	if (!InRecipe || InRecipe->CraftTime <= 0.0f)
	{
		return false;
	}

	// Check station tag requirements
	if (InRecipe->RequiredStationTags.Num() > 0 && !ItemTags.HasAll(InRecipe->RequiredStationTags))
	{
		UE_LOG(LogTemp, Warning, TEXT("Crafting station does not have required tags for recipe %s"), *InRecipe->GetName());
		return false;
	}

	// Check ingredient availability via the recipe execution
	const FArcCraftExecution_Recipe* RecipeExecution = CraftExecution.GetPtr<FArcCraftExecution_Recipe>();
	if (RecipeExecution)
	{
		if (!RecipeExecution->CheckAndConsumeRecipeItems(this, InRecipe, Instigator, false))
		{
			UE_LOG(LogTemp, Warning, TEXT("Recipe ingredient requirements not met for %s"), *InRecipe->GetName());
			return false;
		}
	}

	return true;
}

void UArcCraftComponent::CraftItem(const UArcItemDefinition* InCraftData, UObject* Instigator, int32 Amount, int32 Priority)
{
	const bool bMeetsRequirements = CheckRequirements(InCraftData, Instigator);
	if (!bMeetsRequirements)
	{
		UE_LOG(LogTemp, Warning, TEXT("Crafting requirements not met for %s"), *InCraftData->GetName());
		return;
	}

	CurrentInstigator = Instigator;
	const FArcItemFragment_CraftData* CraftData = InCraftData->FindFragment<FArcItemFragment_CraftData>();

	const bool bIsDisabled = CraftedItemList.Items.IsEmpty();
	
	const FDateTime Time = FDateTime::UtcNow();
	const int64 CurrentTime = Time.ToUnixTimestamp();

	const int32 EndTime = (CraftData->CraftTime * Amount) + CurrentTime;
	FArcCraftItem CraftedItem;

	CraftedItem.Id = FGuid::NewGuid();
	CraftedItem.Recipe = InCraftData;
	CraftedItem.Instigator = Instigator;
	
	CraftedItem.StartTime = CurrentTime;
	CraftedItem.UnpausedEndTime = EndTime;
	CraftedItem.CurrentTime = 0;
	CraftedItem.MaxAmount = Amount;
	CraftedItem.Priority = Priority;
	
	bool bPriorityExists = false;
	int32 LowestPriority = -1;
	for (const FArcCraftItem& Item : CraftedItemList.Items)
	{
		if (Item.Priority == Priority)
		{
			bPriorityExists = true;
		}

		if (Item.Priority > LowestPriority)
		{
			LowestPriority = Item.Priority;
		}
	}

	if (bPriorityExists)
	{
		CraftedItem.Priority = LowestPriority + 1;
	}
	
	if (bIsDisabled)
	{
		int32 Idx = CraftedItemList.Items.Add(CraftedItem);
		PrimaryComponentTick.SetTickFunctionEnable(true);

		OnCraftItemAdded.Broadcast(this, CraftedItemList.Items[Idx]);
	}
	else
	{
		PendingAdds.Add(CraftedItem);
	}
}

void UArcCraftComponent::CraftRecipe(const UArcRecipeDefinition* InRecipe, UObject* Instigator, int32 Amount, int32 Priority)
{
	if (!CheckRecipeRequirements(InRecipe, Instigator))
	{
		UE_LOG(LogTemp, Warning, TEXT("Recipe requirements not met for %s"), *InRecipe->GetName());
		return;
	}

	CurrentInstigator = Instigator;

	const bool bIsDisabled = CraftedItemList.Items.IsEmpty();

	const FDateTime Time = FDateTime::UtcNow();
	const int64 CurrentTimeStamp = Time.ToUnixTimestamp();

	const int32 EndTime = static_cast<int32>(InRecipe->CraftTime * Amount) + CurrentTimeStamp;
	FArcCraftItem CraftedItem;

	CraftedItem.Id = FGuid::NewGuid();
	CraftedItem.Recipe = nullptr; // Recipe-based path doesn't use legacy Recipe field
	CraftedItem.RecipeDefinition = InRecipe;
	CraftedItem.Instigator = Instigator;

	CraftedItem.StartTime = CurrentTimeStamp;
	CraftedItem.UnpausedEndTime = EndTime;
	CraftedItem.CurrentTime = 0;
	CraftedItem.MaxAmount = Amount;
	CraftedItem.Priority = Priority;

	bool bPriorityExists = false;
	int32 LowestPriority = -1;
	for (const FArcCraftItem& Item : CraftedItemList.Items)
	{
		if (Item.Priority == Priority)
		{
			bPriorityExists = true;
		}
		if (Item.Priority > LowestPriority)
		{
			LowestPriority = Item.Priority;
		}
	}

	if (bPriorityExists)
	{
		CraftedItem.Priority = LowestPriority + 1;
	}

	if (bIsDisabled)
	{
		int32 Idx = CraftedItemList.Items.Add(CraftedItem);
		PrimaryComponentTick.SetTickFunctionEnable(true);
		OnCraftItemAdded.Broadcast(this, CraftedItemList.Items[Idx]);
	}
	else
	{
		PendingAdds.Add(CraftedItem);
	}
}

void UArcCraftComponent::OnCraftFinished(const FArcCraftItem& CraftedItem)
{
	if (CraftedItem.RecipeDefinition)
	{
		// Recipe-based path
		const FArcCraftExecution_Recipe* RecipeExecution = CraftExecution.GetPtr<FArcCraftExecution_Recipe>();
		FArcItemSpec NewItemSpec;
		if (RecipeExecution)
		{
			NewItemSpec = RecipeExecution->OnRecipeCraftFinished(this, CraftedItem.RecipeDefinition, CraftedItem.Instigator);
		}
		OnCraftFinishedDelegate.Broadcast(this, nullptr, NewItemSpec);
	}
	else
	{
		// Legacy item-fragment path
		FArcItemSpec NewItemSpec = CraftExecution.GetPtr<FArcCraftExecution>()->OnCraftFinished(this, CraftedItem.Recipe, CraftedItem.Instigator);
		OnCraftFinishedDelegate.Broadcast(this, CraftedItem.Recipe, NewItemSpec);
	}
}

bool UArcCraftComponent::DoesHaveItemsToCraft(const UArcItemDefinition* InRecipe, UObject* InOwner) const
{
	const FArcItemFragment_CraftData* CraftData = InRecipe->FindFragment<FArcItemFragment_CraftData>();
	if (!CraftData)
	{
		return false;
	}
	
	if (!CraftExecution.GetPtr<FArcCraftExecution>()->CheckAndConsumeItems(this, InRecipe, InOwner, false))
	{
		return false;
	}
	

	return true;
}

TArray<FArcCraftItemAmount> UArcCraftComponent::GetAvailableItems(const UArcItemDefinition* InRecipe, UObject* InOwner) const
{
	TArray<FArcCraftItemAmount> OutItems;
	
	const FArcItemFragment_CraftData* CraftData = InRecipe->FindFragment<FArcItemFragment_CraftData>();
	if (!CraftData)
	{
		return TArray<FArcCraftItemAmount>();
	}
	
	OutItems.Append(CraftExecution.GetPtr<FArcCraftExecution>()->AvailableItems(this, InRecipe, InOwner));
	

	return OutItems;
}

// Called every frame
void UArcCraftComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CraftExecution.IsValid())
	{
		return;
	}
	
	const bool bCanCraft = CraftExecution.GetPtr<FArcCraftExecution>()->CanCraft(this, CurrentInstigator.Get());
	if (!bCanCraft)
	{
		return;
	}
	
	for (const FArcCraftItem& Item : PendingAdds)
	{
		int32 Idx = CraftedItemList.Items.Add(Item);
		OnCraftItemAdded.Broadcast(this, CraftedItemList.Items[Idx]);
	}
	
	PendingAdds.Reset(16);
	// ...

	int32 OldHighestPriority = HighestPriorityIndex;
	if (HighestPriorityIndex == INDEX_NONE)
	{
		int32 HighestPriority = INT_MAX;
		for (int32 Idx = 0; Idx < CraftedItemList.Items.Num(); Idx++)
		{
			FArcCraftItem& Item = CraftedItemList.Items[Idx];

			bool bMeetsRequirements = false;
			if (Item.RecipeDefinition)
			{
				bMeetsRequirements = CheckRecipeRequirements(Item.RecipeDefinition, Item.Instigator);
			}
			else if (Item.Recipe)
			{
				bMeetsRequirements = CheckRequirements(Item.Recipe, Item.Instigator);
			}

			if (!bMeetsRequirements)
			{
				const FString ItemName = Item.RecipeDefinition
					? Item.RecipeDefinition->GetName()
					: (Item.Recipe ? Item.Recipe->GetName() : TEXT("Unknown"));
				UE_LOG(LogTemp, Log, TEXT("Selecting Highest Priority item. Crafting requirements not met for %s"), *ItemName);
				continue;
			}

			if (Item.Priority < HighestPriority)
			{
				HighestPriority = Item.Priority;
				HighestPriorityIndex = Idx;
			}
		}

		if (HighestPriorityIndex != INDEX_NONE && HighestPriorityIndex != OldHighestPriority)
		{
			FArcCraftItem& SelectedItem = CraftedItemList.Items[HighestPriorityIndex];

			if (SelectedItem.RecipeDefinition)
			{
				// Recipe path: consume ingredients via recipe execution
				const FArcCraftExecution_Recipe* RecipeExecution = CraftExecution.GetPtr<FArcCraftExecution_Recipe>();
				if (RecipeExecution)
				{
					RecipeExecution->CheckAndConsumeRecipeItems(this, SelectedItem.RecipeDefinition,
						SelectedItem.Instigator, true);
				}
			}
			else
			{
				// Legacy path
				CraftExecution.GetPtr<FArcCraftExecution>()->CheckAndConsumeItems(this, SelectedItem.Recipe,
					SelectedItem.Instigator, true);
			}

			OnCraftStarted.Broadcast(this, SelectedItem);
		}
	}

	if (HighestPriorityIndex == -1)
	{
		// No valid items to craft
		return;
	}

	TArray<int32> PendingRemoves;
	if (CraftedItemList.Items.IsValidIndex(HighestPriorityIndex))
	{
		FArcCraftItem& Item = CraftedItemList.Items[HighestPriorityIndex];

		// Get craft time from recipe or legacy craft data
		float SingleItemCraftTime = 0.0f;
		if (Item.RecipeDefinition)
		{
			SingleItemCraftTime = Item.RecipeDefinition->CraftTime;
		}
		else if (Item.Recipe)
		{
			const FArcItemFragment_CraftData* CraftData = Item.Recipe->FindFragment<FArcItemFragment_CraftData>();
			if (CraftData)
			{
				SingleItemCraftTime = CraftData->CraftTime;
			}
		}

		Item.CurrentTime += DeltaTime;

		if (Item.CurrentTime >= SingleItemCraftTime)
		{
			OnCraftFinished(Item);
			Item.CurrentAmount++;
			Item.CurrentTime = 0;

			if (Item.CurrentAmount >= Item.MaxAmount)
			{
				PendingRemoves.Add(HighestPriorityIndex);
			}

			HighestPriorityIndex = INDEX_NONE;
		}
	}

	for (const int32 Idx : PendingRemoves)
	{
		CraftedItemList.Edit().Remove(Idx);
	}

	if (CraftedItemList.Items.IsEmpty())
	{
		PrimaryComponentTick.SetTickFunctionEnable(false);
	}
	// ...
}
