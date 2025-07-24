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

#include "Components/ActorComponent.h"
#include "ArcItemsArray.h"
#include "ArcMacroDefines.h"
#include "Components/GameFrameworkComponent.h"
#include "Components/GameFrameworkInitStateInterface.h"
#include "Engine/Blueprint.h"
#include "Items/ArcItemTypes.h"
#include "Items/ArcItemId.h"

#include "ArcItemsStoreComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogArcItems, Log, All);

class UArcItemDefinition;
struct FArcItemId;
struct FArcItemData;

UCLASS(BlueprintType)
class ARCCORE_API UArcItemsStoreComponentBlueprint : public UBlueprint
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
	// UBlueprint interface
	virtual bool SupportedByDefaultBlueprintFactory() const override
	{
		return false;
	}
#endif
};

/**
 * Shared store of items, which can be used between multiple items components, on the same owning actor.
 */
UCLASS(Blueprintable, ClassGroup = (Arc), meta = (BlueprintSpawnableComponent))
class ARCCORE_API UArcItemsStoreComponent : public UGameFrameworkComponent, public IGameFrameworkInitStateInterface
{
	GENERATED_BODY()

	friend struct FArcItemsArray;
	friend struct FArcItemData;
	friend struct ArcItems;

	friend struct FArcMoveItemBetweenStoresCommand;
	
private:
	UPROPERTY(Replicated)
	FArcItemsArray ItemsArray;

	UPROPERTY(Replicated)
	int32 ItemNum;

public:
	/**
	 *  If true should use @class UArcItemsStoreSubsystem to store items. Replication in that case should be disabled,
	 *  Implementing, how items are  replicated to client and accessed by server in case of multiplayer is left to subsystem.
	 *
	 *  Testing storing inventories outside of server in some backend service.
	 */
	UPROPERTY(EditAnywhere)
	bool bUseSubsystemForItemStore = false;
	
private:
	TArray<FGameplayTag> LockedSlots;
	
	TMap<FArcItemId, TArray<FGameplayTag>> LockedAttachmentSlots;

	TSet<FArcItemId> LockedItems;

	/**
	 * Functions which can be used to lock editing of slot/item on client side.
	 *
	 * They sould be used from within replication commands during PreSend to and checked in CanSend functions,
	 * to see if Item/slot can be changed.
	 *
	 * This can prevent from excessive changeds o single item, while previous changed have not replicated yet.
	 *
	 * These only work on non authority.
	 */
public:
	void LockItem(const FArcItemId& InItem);
	void UnlockItem(const FArcItemId& InItem);
	bool IsItemLocked(const FArcItemId& InItem) const;
	
	void LockSlot(const FGameplayTag& InSlot);
	void UnlockSlot(const FGameplayTag& InSlot);
	bool IsSlotLocked(const FGameplayTag& InSlot) const;

	void LockAttachmentSlot(const FArcItemId& OwnerId, const FGameplayTag& InSlot);
	void UnlockAttachmentSlot(const FArcItemId& OwnerId, const FGameplayTag& InSlot);
	bool IsAttachmentSlotLocked(const FArcItemId& OwnerId, const FGameplayTag& InSlot) const;

	const FArcItemsArray& GetItemsArray() const
	{
		return ItemsArray;
	}
	
	const FArcItemData* GetItemByDefinition(const UArcItemDefinition* InItemType);

public:
	// Sets default values for this component's properties
	UArcItemsStoreComponent(const FObjectInitializer& ObjectInitializer);

	static const FName NAME_ActorFeatureName;
	virtual FName GetFeatureName() const override { return NAME_ActorFeatureName; }
	
protected:
	virtual void OnRegister() override;

	virtual void InitializeComponent() override;

	// Called when the game starts
	virtual void BeginPlay() override;
	
protected:
	/** Called before item is removed from store. */
	virtual void PreRemove(const FArcItemData* InItem) {};

	/** Called just after item is added to store. */
	virtual void PostAdd(const FArcItemData* InItem) {};

protected:
	/** Initializes item into working state, calling FArcItemData::Initialize and PostAdd() */
	int32 SetupItem(int32 ItemIdx
					, const FArcItemId& OwnerItemId);

public:
	/** Adds new item from ItemSpec. */
	FArcItemId AddItem(const FArcItemSpec& InItem
					   , const FArcItemId& OwnerItemId);

	void AddLoadedItem(UArcItemsStoreComponent* NewItemStoreComponent, FArcItemData&& InItem);

	void MoveItemFrom(const FArcItemId& ItemId, UArcItemsStoreComponent* FromItemsStore);
	
	/**
	 * @brief Takes ItemData and makes copy of it and inserts it into this component.
	 * Does not call any delegates.
	 * @param InItemData ItemData from we want copy
	 */
	TArray<FArcItemId> AddItemDataInternal(FArcItemCopyContainerHelper& InContainer);
	
	/**
	 * @brief Removes items from ItemsStore or reduce stack count.
	 * @param Item Id Of item to remove
	 * @param InItemsComponent Items Component, from which item is removed
	 */
	void RemoveItem(const FArcItemId& Item, int32 Stacks, bool bRemoveOnZeroStacks = true);

	/**
	 * Destroy item along with all attachments
	 * @param ItemId 
	 */
	void DestroyItem(const FArcItemId& ItemId);

	const FArcItemData* GetItemByIdx(int32 Idx) const;

	bool Contains(const UArcItemDefinition* InItemDefinition) const;

public:
	const FArcItemData* GetItemPtr(const FArcItemId& Handle) const;
	FArcItemData* GetItemPtr(const FArcItemId& Handle);

	const TWeakPtr<FArcItemData>& GetWeakItemPtr(const FArcItemId& Handle) const;
	TWeakPtr<FArcItemData>& GetWeakItemPtr(const FArcItemId& Handle);
	
	const FArcItemDataInternal* GetInternalItem(const FArcItemId& InItemId) const;

	TArray<FArcItemCopyContainerHelper> GetAllInternalItems() const;
	FArcItemCopyContainerHelper GetItemCopyHelper(const FArcItemId& InItemId) const;
	
protected:
	const TSharedPtr<FArcItemData>& GetItemSharedPtr(const FArcItemId& Handle) const;
	
	const UArcItemDefinition* GetItemDefinition(FArcItemId ItemId) const
	{
		return ItemsArray[ItemId]->GetItemDefinition();
	}


public:
	void AddItemToSlot(const FArcItemId& InItemId
					   , const FGameplayTag& InSlotId);

	void RemoveItemFromSlot(const FArcItemId& InItemId);

	void ChangeItemSlot(const FArcItemId& InItem, const FGameplayTag& InNewSlot);
	
	const FArcItemData* GetItemFromSlot(const FGameplayTag& InSlotId);
	
	bool IsOnAnySlot(const FArcItemId& InItemId) const;

	TArray<const FArcItemData*> GetItemsAttachedTo(const FArcItemId& InItemId) const;
	const FArcItemData* GetItemAttachedTo(const FArcItemId& InOwnerItemId, const FGameplayTag& AttachSlot) const;

	TArray<const FArcItemDataInternal*> GetInternalAttachtedItems(const FArcItemId& InItemId) const;
	
	TArray<const FArcItemData*> GetAllItemsOnSlots() const;
	/*
	 * Public. Need for some unit tests.
	 */

	int32 GetItemNum() const
	{
		return ItemsArray.Num();
	}
	

	TArray<const FArcItemData*> GetItems() const
	{
		return ItemsArray.GetItemsPtr();
	}
	
	void MarkItemDirtyById(const FArcItemId& InItemId);

	void MarkItemInstanceDirtyById(const FArcItemId& InItemId, UScriptStruct* InScriptStruct);

	// Attachments
	const FArcItemData* FindAttachedItemOnSlot(const FArcItemId& InOwnerId, const FGameplayTag& InAttachSlot) const;
	TArray<const FArcItemData*> GetAttachedItems(const FArcItemId& InOwnerId) const;

	FArcItemId InternalAttachToItemNew(const FArcItemId& OwnerId
		, const UArcItemDefinition* AttachmentDefinition
		, const FGameplayTag& InAttachSlot);
	
	FArcItemId InternalAttachToItem(const FArcItemId& OwnerId
		, const FArcItemId& AttachedId
		, const FGameplayTag& InAttachSlot);

	void DetachItemFrom(const FArcItemId& InAttachmentId);
	void DetachItemFromSlot(const FArcItemId& InOwnerId, const FGameplayTag& InAttachSlot);


	int32 GetItemStacks(const UArcItemDefinition* ItemType) const;

	// Tags
	void AddDynamicTag(const FArcItemId& Item, const FGameplayTag& InTag);
	void RemoveDynamicTag(const FArcItemId& Item, const FGameplayTag& InTag);

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Items")
	void BP_GetItems(TArray<FArcItemDataHandle>& OutItems);

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Items", Meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool BP_GetItem(const FArcItemId& ItemId, FArcItemDataHandle& Item);

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Items", Meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool BP_GetItemFromSlot(const FGameplayTag& ItemSlotId, FArcItemDataHandle& Item);
};
