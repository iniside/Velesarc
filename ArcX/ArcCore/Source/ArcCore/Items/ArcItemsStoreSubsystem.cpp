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



#include "ArcItemsStoreSubsystem.h"

#include "Core/ArcCoreAssetManager.h"

void UArcItemsStoreSubsystem::SetupItem(FArcItemData* ItemPtr, const FArcItemSpec& InSpec)
{
	const UArcItemDefinition* ItemDef = UArcCoreAssetManager::Get().GetAsset<UArcItemDefinition>(InSpec.GetItemDefinitionId());
    
    ItemPtr->SetItemSpec(InSpec);
    
    TArray<const FArcItemFragment_ItemInstanceBase*> InstancesToMake = ItemDef->GetFragmentsOfType<FArcItemFragment_ItemInstanceBase>();
    for (const FArcItemFragment_ItemInstanceBase* IIB : InstancesToMake)
    {
    	if (IIB->GetCreateItemInstance() == false)
    	{
    		continue;
    	}
    	
    	if (UScriptStruct* InstanceType = IIB->GetItemInstanceType())
    	{
    		int32 InitialDataIdx = InSpec.InitialInstanceData.IndexOfByPredicate([InstanceType](const TSharedPtr<FArcItemInstance>& Other)
    		{
    			return Other->GetScriptStruct() == InstanceType;
    		});
    		
    		int32 Idx = ItemPtr->ItemInstances.Data.AddDefaulted();
    		{
    			TSharedPtr<FArcItemInstance> SharedPtr = ArcItems::AllocateInstance(InstanceType);

    			if (InitialDataIdx != INDEX_NONE)
    			{
    				InstanceType->GetCppStructOps()->Copy(SharedPtr.Get(), InSpec.InitialInstanceData[InitialDataIdx].Get(), 0);
    			}

    			ItemPtr->ItemInstances.Data[Idx] = SharedPtr;
    		}
    	}
    }
    
    TArray<const FArcItemFragment_ItemInstanceBase*> SpecFragments = InSpec.GetSpecFragments();
    for (const FArcItemFragment_ItemInstanceBase* Fragment : SpecFragments)
    {
    	if (Fragment->GetCreateItemInstance() == false)
    	{
    		continue;
    	}
    
    	if (UScriptStruct* InstanceType = Fragment->GetItemInstanceType())
    	{
    		int32 InitialDataIdx = InSpec.InitialInstanceData.IndexOfByPredicate([InstanceType](const TSharedPtr<FArcItemInstance>& Other)
    		{
    			return Other->GetScriptStruct() == InstanceType;
    		});
    		
    		TSharedPtr<FArcItemInstance> SharedPtr = ArcItems::AllocateInstance(InstanceType);
    		if (InitialDataIdx != INDEX_NONE)
    		{
    			InstanceType->GetCppStructOps()->Copy(SharedPtr.Get(), InSpec.InitialInstanceData[InitialDataIdx].Get(), 0);
    		}

    		int32 Idx = ItemPtr->ItemInstances.Data.AddDefaulted();
    		ItemPtr->ItemInstances.Data[Idx] = SharedPtr;
    	}
    }
    
    ItemPtr->SetItemInstances(ItemPtr->ItemInstances);
}

FArcItemId UArcItemsStoreSubsystem::AddItem(const FArcItemSpec& InItem, const FArcItemId& OwnerItemId, UArcItemsStoreComponent* InComponent)
{
	TSharedPtr<FArcItemData> NewEntry = FArcItemData::NewFromSpec(InItem);
	SetupItem(NewEntry.Get(), InItem);
	NewEntry->Initialize(InComponent);
	
	ItemsMap.Add(NewEntry->GetItemId(), NewEntry);
	
	return NewEntry->GetItemId();
}

FArcItemId UArcItemsStoreSubsystem::AddItem(FArcItemCopyContainerHelper& InContainer)
{
	return FArcItemId();
}

FArcItemCopyContainerHelper UArcItemsStoreSubsystem::GetItem(const FArcItemId& InItemId) const
{
	if (const TSharedPtr<FArcItemData>* Item = ItemsMap.Find(InItemId))
	{
		FArcItemCopyContainerHelper ItemCopyContainer = FArcItemCopyContainerHelper::New(Item->Get());

		const TArray<FArcItemId>& AttachedItem = (*Item)->AttachedItems;
		for (const FArcItemId& AttachedItemId : AttachedItem)
		{
			if (const TSharedPtr<FArcItemData>* AttachedItemPtr = ItemsMap.Find(AttachedItemId))
			{
				
			}
		}
		
		return ItemCopyContainer;
	}
	
	return FArcItemCopyContainerHelper();
}

const FArcItemData* UArcItemsStoreSubsystem::GetItemPtr(const FArcItemId& InItemId) const
{
	if (const TSharedPtr<FArcItemData>* ItemPtr = ItemsMap.Find(InItemId))
	{
		return ItemPtr->Get();
	}
	
	return nullptr;
}

FArcItemData* UArcItemsStoreSubsystem::GetItemPtr(const FArcItemId& InItemId)
{
	if (TSharedPtr<FArcItemData>* ItemPtr = ItemsMap.Find(InItemId))
	{
		return ItemPtr->Get();
	}
	
	return nullptr;
}
