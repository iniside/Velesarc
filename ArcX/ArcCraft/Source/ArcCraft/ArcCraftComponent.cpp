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

#include "Items/ArcItemsStoreComponent.h"
#include "Net/UnrealNetwork.h"

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

void UArcCraftComponent::CraftItem(const UArcCraftData* InCraftData, int32 Amount, int32 Priority)
{
	const bool bIsDisabled = CraftedItemList.Items.IsEmpty();
	
	const FDateTime Time = FDateTime::UtcNow();
	const int64 CurrentTime = Time.ToUnixTimestamp();

	const int32 EndTime = (InCraftData->CraftTime * Amount) + CurrentTime;
	FArcCraftItem CraftedItem;

	CraftedItem.Id = FGuid::NewGuid();
	CraftedItem.Recipe = InCraftData;

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
	UArcItemsStoreComponent* ItemsStore = GetItemsStore();
	if (!ItemsStore)
	{
		return;
	}

	FArcItemSpec ItemSpec = FArcItemSpec::NewItem(CraftedItem.Recipe->ItemDefinition, 1, 1);
	ItemsStore->AddItem(ItemSpec, FArcItemId());
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

UArcItemsStoreComponent* UArcCraftComponent::GetItemsStore() const
{
	if (ItemsStoreComponent.IsValid())
	{
		return ItemsStoreComponent.Get();
	}

	ItemsStoreComponent = GetOwner()->FindComponentByClass<UArcItemsStoreComponent>();
	return ItemsStoreComponent.Get();
}



