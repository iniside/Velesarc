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

#include "ArcItemFragment_ItemStats.h"

#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/ArcItemsStoreComponent.h"

void FArcItemFragment_ItemStats::OnItemInitialize(const FArcItemData* InItem) const
{
	UpdateStats(InItem);
}

void FArcItemFragment_ItemStats::OnItemChanged(const FArcItemData* InItem) const
{
	UpdateStats(InItem);
}

void FArcItemFragment_ItemStats::UpdateStats(const FArcItemData* InItem) const
{
	FArcItemInstance_ItemStats* MyInstance = ArcItems::FindMutableInstance<FArcItemInstance_ItemStats>(InItem);
	if (InItem->GetItemsStoreComponent()->GetOwner()->HasAuthority())
	{
		const TArray<const FArcItemData*>& ItemsInSockets = InItem->GetItemsInSockets();

		MyInstance->ReplicatedItemStats.Empty();
		
		for (const FArcItemAttributeStat& S : DefaultStats)
		{
			int32 Idx = MyInstance->ReplicatedItemStats.IndexOfByKey(S.Attribute);
			if (Idx == INDEX_NONE)
			{
				FArcItemStatReplicated NewStat;
				NewStat.Attribute = S.Attribute.GetUProperty()->GetFName();
				Idx = MyInstance->ReplicatedItemStats.Add(NewStat);
			}
			
			FArcStat US;
			US.ItemId = InItem->GetItemId();
			US.Value = S.Value.GetValue();
			US.Type = S.Type;

			MyInstance->ReplicatedItemStats[Idx].Stats.Add(US);
			MyInstance->ReplicatedItemStats[Idx].Recalculate();
		}
		
		for (const FArcItemData* SocketItem : ItemsInSockets)
		{
			const FArcItemFragment_ItemStats* SocketItemStats = ArcItems::FindFragment<FArcItemFragment_ItemStats>(SocketItem);
			if (SocketItemStats == nullptr)
			{
				continue;
			}
			
			for (const FArcItemAttributeStat& ItemAttributeStat : SocketItemStats->DefaultStats)
			{
				int32 Idx = MyInstance->ReplicatedItemStats.IndexOfByKey(ItemAttributeStat.Attribute);
				if (Idx == INDEX_NONE)
				{
					FArcItemStatReplicated NewStat;
					NewStat.Attribute = ItemAttributeStat.Attribute.GetUProperty()->GetFName();
					Idx = MyInstance->ReplicatedItemStats.Add(NewStat);
				}
				
				int32 StatIdx = MyInstance->ReplicatedItemStats[Idx].Stats.IndexOfByKey(SocketItem->GetItemId());
				if (StatIdx != INDEX_NONE)
				{
					continue;
				}
				
				FArcStat US;
				US.ItemId = SocketItem->GetItemId();
				US.Value = ItemAttributeStat.Value.GetValue();
				US.Type = ItemAttributeStat.Type;
			
				MyInstance->ReplicatedItemStats[Idx].Stats.Add(US);
				MyInstance->ReplicatedItemStats[Idx].Recalculate();
			}
		}
	}
	
	MyInstance->Stats.Empty();
	for (const FArcItemStatReplicated& Rep : MyInstance->ReplicatedItemStats)
	{
		MyInstance->Stats.FindOrAdd(Rep.Attribute) = Rep.FinalValue;
	}
}

void FArcItemFragment_ItemStats::UpdateStatsInstance(FArcItemInstance_ItemStats* Instance, const FArcItemData* InItem) const
{
	const TArray<const FArcItemData*>& ItemsInSockets = InItem->GetItemsInSockets();

	Instance->ReplicatedItemStats.Empty();
	for (const FArcItemAttributeStat& S : DefaultStats)
	{
		int32 Idx = Instance->ReplicatedItemStats.IndexOfByKey(S.Attribute);
		if (Idx == INDEX_NONE)
		{
			FArcItemStatReplicated NewStat;
			NewStat.Attribute = S.Attribute.GetUProperty()->GetFName();
			Idx = Instance->ReplicatedItemStats.Add(NewStat);
		}
		
		FArcStat US;
		US.ItemId = InItem->GetItemId();
		US.Value = S.Value.GetValue();
		US.Type = S.Type;

		Instance->ReplicatedItemStats[Idx].Stats.Add(US);
		Instance->ReplicatedItemStats[Idx].Recalculate();
		Instance->Stats.Empty();
	}
	
	for (const FArcItemStatReplicated& Rep : Instance->ReplicatedItemStats)
	{
		Instance->Stats.FindOrAdd(Rep.Attribute) = Rep.FinalValue;
	}
}
