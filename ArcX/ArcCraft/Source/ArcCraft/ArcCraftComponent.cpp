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

#include "Core/ArcCoreAssetManager.h"
#include "Items/ArcItemsStoreComponent.h"
#include "Net/UnrealNetwork.h"

bool FArcCraftRequirement_ItemDefinitionCount::MeetsRequirement(UArcCraftComponent* InCraftComponent, const UArcCraftData* InCraftData, const UObject* InOwner) const
{
	const AActor* OwnerActor = Cast<AActor>(InOwner);
	if (!OwnerActor)
	{
		return false;
	}

	UArcItemsStoreComponent* ItemsStore = OwnerActor->FindComponentByClass<UArcItemsStoreComponent>();
	if (!ItemsStore)
	{
		return false;
	}

	for (const FArcCraftItemDefinitionCount& ItemDefCount : ItemDefinitions)
	{
		const FArcItemData* ItemData = ItemsStore->GetItemByDefinition(ItemDefCount.ItemDefinition);
		if (!ItemData)
		{
			return false; // Item definition not found in store
		}

		const uint16 Stacks = ItemData->GetStacks();
		if (ItemDefCount.Count > Stacks)
		{
			return false; // Not enough items of this type
		}
	}
}

void FArcCraftExecution_MakeItemSpec::OnCraftFinished(UArcCraftComponent* InCraftComponent, const UArcCraftData* InCraftData, const UObject* InOwner
	, FArcItemSpec& ItemSpec) const
{
	int32 SpecIdx = InCraftComponent->GetCraftedItemSpecList().Items.IndexOfByKey(InCraftData->ItemDefinition.AssetId);
	if (SpecIdx != INDEX_NONE)
	{
		ItemSpec = InCraftComponent->GetCraftedItemSpecList().Items[SpecIdx];
		ItemSpec.Amount++;
		return;
	}
	
	ItemSpec = FArcItemSpec::NewItem(InCraftData->ItemDefinition, 1, 1);
}

void FArcCraftExecution_AddToCraftingStore::OnCraftFinished(UArcCraftComponent* InCraftComponent, const UArcCraftData* InCraftData
															, const UObject* InOwner, FArcItemSpec& ItemSpec) const
{
	if (!ItemSpec.GetItemDefinitionId().IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Crafting execution failed: ItemSpec is invalid."));
		return;
	}

	int32 SpecIdx = InCraftComponent->GetCraftedItemSpecList().Items.IndexOfByKey(InCraftData->ItemDefinition.AssetId);
	if (SpecIdx != INDEX_NONE)
	{
		InCraftComponent->GetCraftedItemSpecList().Items[SpecIdx].Amount = ItemSpec.Amount;
		return;
	}

	InCraftComponent->GetCraftedItemSpecList().Edit().Add(ItemSpec);
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
	DOREPLIFETIME_CONDITION(UArcCraftComponent, CraftedItemSpecList, COND_OwnerOnly);
}

void UArcCraftComponent::UpdateStatus()
{
	const FDateTime Time = FDateTime::UtcNow();
	const int64 CurrentTime = Time.ToUnixTimestamp();
	
	for (FArcCraftItem& Item : CraftedItemList.Items)
	{
		const int32 OneItemCraftTime = Item.Recipe->CraftTime;
		const int64 ElapsedTime = CurrentTime - Item.StartTime;

		// How much items cloud we created in the time that elapsed since we last checked ?
		int32 PossiblyCraftedItems = ElapsedTime / OneItemCraftTime;
	}
}

void UArcCraftComponent::CraftItem(const UArcCraftData* InCraftData, UObject* Instigator, int32 Amount, int32 Priority)
{
	for (const TInstancedStruct<FArcCraftRequirement>& Requirement : InCraftData->Requirements)
	{
		if (!Requirement.GetPtr<FArcCraftRequirement>()->MeetsRequirement(this, InCraftData, Instigator))
		{
			UE_LOG(LogTemp, Warning, TEXT("Crafting requirements not met for %s"), *InCraftData->GetName());
			return;
		}
	}
	
	const bool bIsDisabled = CraftedItemList.Items.IsEmpty();
	
	const FDateTime Time = FDateTime::UtcNow();
	const int64 CurrentTime = Time.ToUnixTimestamp();

	const int32 EndTime = (InCraftData->CraftTime * Amount) + CurrentTime;
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
		CraftedItemList.Items.Add(CraftedItem);
		PrimaryComponentTick.SetTickFunctionEnable(true);	
	}
	else
	{
		PendingAdds.Add(CraftedItem);
	}
}

void UArcCraftComponent::OnCraftFinished(const FArcCraftItem& CraftedItem)
{
	FArcItemSpec NewItemSpec;
	for (const TInstancedStruct<FArcCraftExecution>& Requirement : CraftedItem.Recipe->OnFinishedExecutions)
	{
		Requirement.GetPtr<FArcCraftExecution>()->OnCraftFinished(this, CraftedItem.Recipe, CraftedItem.Instigator, NewItemSpec);
	}
}

// Called every frame
void UArcCraftComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	for (const FArcCraftItem& Item : PendingAdds)
	{
		CraftedItemList.Items.Add(Item);
	}
	
	PendingAdds.Reset(16);
	// ...

	int32 HighestPriority = INT_MAX;
	int32 HighestPriorityIndex = -1;
	for (int32 Idx = 0; Idx < CraftedItemList.Items.Num(); Idx++)
	{
		FArcCraftItem& Item = CraftedItemList.Items[Idx];
		if (Item.Priority < HighestPriority)
		{
			HighestPriority = Item.Priority;
			HighestPriorityIndex = Idx;
		}
	}
	
	TArray<int32> PendingRemoves;
	//for (int32 Idx = 0; Idx < CraftedItemList.Items.Num(); Idx++)
	if (CraftedItemList.Items.IsValidIndex(HighestPriorityIndex))
	{
		FArcCraftItem& Item = CraftedItemList.Items[HighestPriorityIndex];
		const float SingleItemCraftTime = Item.Recipe->CraftTime;
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
