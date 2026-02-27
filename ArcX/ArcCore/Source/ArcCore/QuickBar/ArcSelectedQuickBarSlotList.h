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

#pragma once
#include "GameplayTagContainer.h"
#include "Iris/ReplicationState/IrisFastArraySerializer.h"
#include "Items/ArcItemId.h"
#include "Net/Serialization/FastArraySerializer.h"

#include "Iris/ReplicationState/Private/IrisFastArraySerializerInternal.h"

#include "ArcSelectedQuickBarSlotList.generated.h"

class UArcQuickBarComponent;

USTRUCT()
struct FArcSelectedQuickBarSlot : public FFastArraySerializerItem
{
	GENERATED_BODY()
 
public:
	UPROPERTY(Transient)
	uint32 InvalidationHandle = 0;
	
	UPROPERTY(Transient)
	FGameplayTag BarId;
 
	UPROPERTY(Transient)
	FGameplayTag QuickSlotId;

	// Item Id assigned to this QuickSlot. Optional.
	UPROPERTY(Transient)
	FArcItemId AssignedItemId;

	// Item Slot assigned to this QuickSlot. Optional.
	UPROPERTY(Transient)
	FGameplayTag ItemSlot;

	// Quick Slots can still be replicated but not selected.
	UPROPERTY(Transient)
	bool bIsSlotActive = false;

	FDelegateHandle WaitItemAddToSlotHandle;

	void PreReplicatedRemove(const struct FArcSelectedQuickBarSlotList& InArraySerializer);

	void PostReplicatedAdd(const struct FArcSelectedQuickBarSlotList& InArraySerializer);

	bool operator==(const FArcSelectedQuickBarSlot& Other) const
	{
		return BarId == Other.BarId
		&& QuickSlotId == Other.QuickSlotId
		&& AssignedItemId == Other.AssignedItemId
		&& ItemSlot == Other.ItemSlot
		&& InvalidationHandle == Other.InvalidationHandle
		&& bIsSlotActive == Other.bIsSlotActive;
	}
	
	FArcSelectedQuickBarSlot& ActivateQuickSlot()
	{
		bIsSlotActive = true;

		return *this;
	}
	
	FArcSelectedQuickBarSlot& DeactivateQuickSlot()
	{
		bIsSlotActive = false;

		return *this;
	}
	
	FArcSelectedQuickBarSlot& SetItemSlot(const FGameplayTag& InItemSlot)
	{
		ItemSlot = InItemSlot;

		return *this;
	}
	
	FArcSelectedQuickBarSlot& SetAssignedItemId(const FArcItemId& InItemId)
	{
		AssignedItemId = InItemId;

		return *this;
	}
};

template <>
struct TStructOpsTypeTraits<FArcSelectedQuickBarSlot> : public TStructOpsTypeTraitsBase2<FArcSelectedQuickBarSlot>
{
	enum
	{
		WithIdenticalViaEquality = true
	};
};

/**
 * Replicates selected slots as well as ItemIds / ItemSlots assigned to them.
 * 
 */
USTRUCT()
struct ARCCORE_API FArcSelectedQuickBarSlotList : public FIrisFastArraySerializer
{
	GENERATED_BODY()
 
	friend struct FArcSelectedQuickBarSlot;
	
protected:
	UPROPERTY(Transient)
	TArray<FArcSelectedQuickBarSlot> Items;

	struct PendingSlot
	{
		FArcItemId ItemId;
		FGameplayTag BarId;
		FGameplayTag QuickSlotId;

		bool operator==(const PendingSlot& Other) const
		{
			return ItemId == Other.ItemId;
		}

		bool operator==(const FArcItemId& Other) const
		{
			return ItemId == Other;
		}
	};
	mutable TArray<PendingSlot> PendingAddQuickSlotItems;
	
public:
	TWeakObjectPtr<UArcQuickBarComponent> QuickBar;
		
	using ItemArrayType = TArray<FArcSelectedQuickBarSlot>;

	const ItemArrayType& GetItemArray() const
	{
		return Items;
	}

	ItemArrayType& GetItemArray()
	{
		return Items;
	}

	using FFastArrayEditor = UE::Net::TIrisFastArrayEditor<FArcSelectedQuickBarSlotList>;

	FFastArrayEditor Edit()
	{
		return FFastArrayEditor(*this);
	}
	
	FArcSelectedQuickBarSlot& AddQuickSlot(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId, const FArcItemId& ItemId, const FGameplayTag& InItemSlot)
	{
		const int32 Index = IndexOf(BarId, QuickSlotId);
		if (Index != INDEX_NONE)
		{
			return Items[Index];
		}

		FArcSelectedQuickBarSlot NewSelectedSlot;
		NewSelectedSlot.BarId = BarId;
		NewSelectedSlot.QuickSlotId = QuickSlotId;
		NewSelectedSlot.AssignedItemId = ItemId;
		NewSelectedSlot.ItemSlot = InItemSlot;
		
		static uint32 InvalidationHandle = 0;
		InvalidationHandle++;
		NewSelectedSlot.InvalidationHandle = InvalidationHandle;
		
		Items.Add(NewSelectedSlot);
		const int32 NewIndex = Items.Num() -1;
		
		return Items[NewIndex];
	}

	void HandlePendingAddedQuickSlots();

	void RemoveQuickSlot(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId);

	void ActivateQuickSlot(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId);

	bool IsQuickSlotSelected(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId) const;

	void DeactivateQuickSlot(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId);

	TArray<FGameplayTag> GetAllActiveSlots(const FGameplayTag& InBarId) const;

	FGameplayTag FindFirstSelectedSlot(const FGameplayTag& InBarId) const;

	int32 IndexOf(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId) const;

	FArcSelectedQuickBarSlot* Find(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId);

	const FArcSelectedQuickBarSlot* Find(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId) const;

	const FGameplayTag FindItemSlot(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId) const;

	const FArcItemId FindItemId(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId) const;

	bool IsItemOnAnySlot(const FArcItemId& InItemId) const;

	// BarId, QuickSlotId
	TPair<FGameplayTag, FGameplayTag> FindItemBarAndSlot(const FArcItemId& InItemId) const;

	const TArray<FArcSelectedQuickBarSlot>& GetSelections() const { return Items; }
};
