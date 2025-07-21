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

#include "ArcGameplayEffectContext.h"

#include "Items/ArcItemDefinition.h"
#include "ArcCore/Items/ArcItemData.h"
#include "ArcCore/Items/ArcItemsComponent.h"

#include "MassCommonTypes.h"
#include "Items/ArcItemsStoreComponent.h"

void FArcGameplayEffectContext::SetSourceItemHandle(FArcItemId InSourceItemHandle)
{
	SourceItemId = InSourceItemHandle;
	UArcItemsStoreComponent* AuI = Cast<UArcItemsStoreComponent>(GetSourceObject());
	SourceItemPtr = AuI->GetItemPtr(SourceItemId);
	SourceItem = SourceItemPtr->GetItemDefinition();
}

const FArcItemData* FArcGameplayEffectContext::GetSourceItemPtr() const
{
	if (SourceItemPtr)
	{
		return SourceItemPtr;
	}

	UArcItemsStoreComponent* ItemsStore = Cast<UArcItemsStoreComponent>(GetSourceObject());
	return SourceItemPtr = ItemsStore->GetItemPtr(GetSourceItemHandle());
}

const TWeakPtr<FArcItemData> FArcGameplayEffectContext::GetSourceItemWeakPtr() const
{
	UArcItemsStoreComponent* ItemsStore = Cast<UArcItemsStoreComponent>(GetSourceObject());
	return ItemsStore->GetWeakItemPtr(GetSourceItemHandle());
}

const UArcItemDefinition* FArcGameplayEffectContext::GetSourceItem() const
{
	if (SourceItem)
	{
		return SourceItem;	
	}
	
	if (GetSourceItemPtr())
	{
		SourceItem = GetSourceItemPtr()->GetItemDefinition();
	}

	return SourceItem;
}

bool FArcGameplayEffectContext::NetSerialize(FArchive& Ar
											 , class UPackageMap* Map
											 , bool& bOutSuccess)
{
	FGameplayEffectContext::NetSerialize(Ar
		, Map
		, bOutSuccess);
	if (Ar.IsSaving())
	{
		Ar << TargetDataHandle;
		{
			bool bValid2 = SourceItemId.IsValid();
			Ar.SerializeBits(&bValid2
				, 1);
			if (bValid2)
			{
				SourceItemId.NetSerialize(Ar
					, Map
					, bOutSuccess);
			}
		}
	}
	if (Ar.IsLoading())
	{
		Ar << TargetDataHandle;
		UArcItemsStoreComponent* AuI = Cast<UArcItemsStoreComponent>(GetSourceObject());
		{
			bool bValid2 = false;
			Ar.SerializeBits(&bValid2
				, 1);
			if (bValid2)
			{
				SourceItemId.NetSerialize(Ar
					, Map
					, bOutSuccess);
			}
			if (AuI && SourceItemId.IsValid())
			{
				SourceItemPtr = AuI->GetItemPtr(SourceItemId);
				if (SourceItemPtr)
				{
					SourceItem = SourceItemPtr->GetItemDefinition();
				}
			}
		}
	}
	bOutSuccess = true;
	return true;
}
