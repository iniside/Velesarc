/**
 * This file is part of ArcX.
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

#include "ArcMacroDefines.h"
#include "Components/ActorComponent.h"

#include "Engine/Blueprint.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "Iris/ReplicationState/IrisFastArraySerializer.h"
#include "Iris/ReplicationState/Private/IrisFastArraySerializerInternal.h"
#include "Items/ArcItemId.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/WorldSubsystem.h"

#include "ArcQuickBarComponent.generated.h"

class UArcCoreAbilitySystemComponent;
class UArcItemsStoreComponent;

struct FArcItemData;
struct FArcItemData;
struct FGameplayAbilitySpec;
struct FArcItemDataHandle;

DECLARE_LOG_CATEGORY_EXTERN(LogArcQuickBar
	, Log
	, All);

/**
 * Struct representing single QuickSlot on QuickBar.
 *
 * It contains several configuration options.
 *
 * If @link FArcQuickBarSlot::ItemSlotId is set, this slot will automatically listen for
 * item added to this slot, and if added it will be automatically added to @link UArcQuickBarComponent::ItemSlotMapping
 * but nothing else will happen.
 *
 * If @link FArcQuickBarSlot::InputBind have @link FGF1047QuickBarInputBindHandling::ItemSlotId set
 * it will listen for the @link FArcQuickBarInputBindHandling::ItemSlotId and if item is added to this slot
 * applicable input tags will be added.
 *
 * If you need items to have their inputs automatically assigned by QuickBar when they are added to ItemSlot
 * assign @link FArcQuickBarSlot::ItemSlotId and @link FArcQuickBarInputBindHandling::ItemSlotId.
 *
 * If you want to handle it manually later (like when QuickSlot or QuickBar changes its state), only assign
 * @link FArcQuickBarSlot::ItemSlotId.
 *
 * If you want to handle everything manually (like allow Player to drag items to QuickBar), leve both empty.
 */
USTRUCT(BlueprintType)
struct ARCCORE_API FArcQuickBarSlot
{
	GENERATED_BODY()

public:
	/**
	 * If true it will be automatically set in selected/deselected state when item is added to Slot which is mapped
	 * to this QuickSlot.
	 */
	UPROPERTY(EditAnywhere, Category = "Arc Core")
	bool bAutoSelect = true;
	
	UPROPERTY(EditAnywhere, Category = "Arc Core")
	bool bCanByCycled = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arc Core", meta = (Categories = "QuickSlot"))
	FGameplayTag QuickBarSlotId;

	/**
	 * Optional. This is the ItemSlot (as on ItemSlotComponent), to which this quick bar slot maps.
	 * If set it will register delegate and when item is added to slot and replicated it will be added to
	 * @link UArcQuickBarComponent::ItemSlotMapping
	 * If this slot is bound to delegates it will also call
	 * @link FArcQuickBarInputBindHandling::OnAddedToQuickBar
	 * @link FArcQuickBarInputBindHandling::OnRemovedFromQuickBar
	 *
	 * TODO: Also call handlers ?
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arc Core", meta = (Categories = "SlotId"))
	FGameplayTag ItemSlotId;

	/**
	 * 
	 */
	UPROPERTY(EditAnywhere, Category = "Arc Core", meta = (BaseStruct = "/Script/ArcCore.ArcQuickBarInputBindHandling"))
	FInstancedStruct InputBind;

	UPROPERTY(EditAnywhere, Category = "Arc Core", meta = (BaseStruct = "/Script/ArcCore.ArcQuickBarSlotValidator", ShowTreeView, ExcludeBaseStruct))
	TArray<FInstancedStruct> Validators;

	/**
	 * Slot Handlers are called when slots are selected. They are most useful, when we can only have
	 * one slot on bar active at time.
	 *
	 * Although they are always called even if we activate multiple slots on single bar at the same time.
	 * If doing so, make sure that multiple handlers activated at the same time do not conflict with
	 * each other (like all of them trying to play animation).
	 */
	UPROPERTY(EditAnywhere, Category = "Arc Core", meta = (BaseStruct = "/Script/ArcCore.ArcQuickSlotHandler", ShowTreeView, ExcludeBaseStruct))
	TArray<FInstancedStruct> SelectedHandlers;

	bool operator==(const FArcQuickBarSlot& Other) const
	{
		return QuickBarSlotId == Other.QuickBarSlotId;
	}

	bool operator==(const FGameplayTag& Other) const
	{
		return QuickBarSlotId == Other;
	}
};

UENUM()
enum class EArcQuickSlotsMode : uint8
{
	// Slots can be cycled and auto selcted, when item is added.
	Cyclable,

	// Slots can never be cycled or auto selected.
	ActivateOnly,
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcQuickBar
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arc Core")
	TSubclassOf<UArcItemsStoreComponent> ItemsStoreClass;

	UPROPERTY(EditAnywhere, Category = "Arc Core")
	EArcQuickSlotsMode QuickSlotsMode = EArcQuickSlotsMode::Cyclable;

	/**
	 * If true Quick Slots must have ItemSlotId set.
	 * When new Item is added to ItemSlotId, it will be automatically selected and input will be bound.
	 * For Cyclebale QuickSlots only slot with bAutoSelect will be selcted.
	 *
	 * for ActivateOnly any slot will run slected handler and bind input.
	 */
	UPROPERTY(EditAnywhere, Category = "Arc Core")
	bool bCanAutoSelectOnItemAddedToSlot = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arc Core", meta = (Categories = "QuickBar"))
	FGameplayTag BarId;

	/*
	 * Actions called when SelectBar or CycleBarForward is executed.
	 */
	UPROPERTY(EditAnywhere, Category = "Arc Core", meta = (BaseStruct = "/Script/ArcCore.ArcQuickBarSelectedAction", ShowTreeView, ExcludeBaseStruct))
	TArray<FInstancedStruct> QuickBarSelectedActions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arc Core")
	TArray<FArcQuickBarSlot> Slots;

	bool operator==(const FArcQuickBar& Other) const
	{
		return BarId == Other.BarId;
	}

	bool operator==(const FGameplayTag& Other) const
	{
		return BarId == Other;
	}
};

USTRUCT()
struct FArcSelectedQuickBarSlot : public FFastArraySerializerItem
{
	GENERATED_BODY()
 
public:
	UPROPERTY(Transient)
	FGameplayTag BarId;
 
	UPROPERTY(Transient)
	FGameplayTag SlotId;

	// Item Id assigned to this QuickSlot. Optional.
	UPROPERTY(Transient)
	FArcItemId AssignedItemId;

	UPROPERTY(Transient)
	FArcItemId OldAssignedItemId;
	
	// Item Slot assigned to this QuickSlot. Optional.
	UPROPERTY(Transient)
	FGameplayTag ItemSlot;

	// Quick Slots can still be replicated but not selected.
	UPROPERTY(Transient)
	bool bIsSelected = false;

	UPROPERTY(NotReplicated, Transient)
	bool bFromServer = false;
	
	UPROPERTY(NotReplicated, Transient)
	bool bClientSelected = false;

	UPROPERTY(NotReplicated, Transient)
	bool bClientDeselected = false;

	FDelegateHandle WaitItemAddToSlotHandle;

	bool operator==(const FArcSelectedQuickBarSlot& Other) const
	{
		return BarId == Other.BarId
		&& SlotId == Other.SlotId
		&& AssignedItemId == Other.AssignedItemId
		&& OldAssignedItemId == Other.OldAssignedItemId
		&& ItemSlot == Other.ItemSlot;
	}
	
	void PreReplicatedRemove(const FArcSelectedQuickBarSlotList& SelectionMap);

	void PostReplicatedAdd(const FArcSelectedQuickBarSlotList& SelectionMap);
	bool HandlePendingItemAdded(const FArcSelectedQuickBarSlotList& SelectionMap);
	
	void PostReplicatedChange(const FArcSelectedQuickBarSlotList& SelectionMap);

	FArcSelectedQuickBarSlot& SetQuickSlotClientSelected(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId)
	{
		bClientSelected = true;
		return *this;
	}

	FArcSelectedQuickBarSlot& SetQuickSlotClientDeselected(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId)
	{
		bClientDeselected = true;
		return *this;
	}
	
	FArcSelectedQuickBarSlot& SetQuickSlotSelected(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId)
	{
		bIsSelected = true;

		return *this;
	}
	
	FArcSelectedQuickBarSlot& SetQuickSlotDeselected(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId)
	{
		bIsSelected = false;

		return *this;
	}
	
	FArcSelectedQuickBarSlot& SetItemSlot(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId, const FGameplayTag& InItemSlot)
	{
		ItemSlot = InItemSlot;

		return *this;
	}
	
	FArcSelectedQuickBarSlot& SetAssignedItemId(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId, const FArcItemId& InItemId)
	{
		OldAssignedItemId = AssignedItemId;
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
struct FArcSelectedQuickBarSlotList : public FIrisFastArraySerializer
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
	
public:
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices
							  , int32 FinalSize);
	
	//void Init(class UArcQuickBarComponent* InQuickBar);
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

	void AddPendingAddQuickSlotItem(const FArcItemId& InItemId, const FGameplayTag& InBarId, const FGameplayTag& InQuickSlotId) const
	{
		PendingAddQuickSlotItems.AddUnique({InItemId, InBarId, InQuickSlotId});
	}

	void HandleOnItemAddedToSlot();
	
	FArcSelectedQuickBarSlot& AddQuickSlot(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId)
	{
		const int32 Index = IndexOf(BarId, QuickSlotId);
		if (Index != INDEX_NONE)
		{
			Edit().Edit(Index);
			return Items[Index];
		}

		FArcSelectedQuickBarSlot NewSelectedSlot;
		NewSelectedSlot.BarId = BarId;
		NewSelectedSlot.SlotId = QuickSlotId;

		Items.Add(NewSelectedSlot);
		const int32 NewIndex = Items.Num() -1;
		Edit().Edit(NewIndex);
		return Items[NewIndex];
	}
	
	void RemoveQuickSlot(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId)
	{
		for (int32 Idx = 0; Idx < Items.Num(); Idx++)
		{
			if (Items[Idx].BarId == BarId && Items[Idx].SlotId == QuickSlotId)
			{
				Edit().Remove(Idx);
				return;
			}
		}
	}

	void SetQuickSlotClientSelected(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId)
	{
		const int32 Index = IndexOf(BarId, QuickSlotId);
		if (Index == INDEX_NONE)
		{
			return;
		}

		Items[Index].bClientSelected = true;

		Edit().Edit(Index);
	}

	void SetQuickSlotClientDeselected(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId)
	{
		const int32 Index = IndexOf(BarId, QuickSlotId);
		if (Index == INDEX_NONE)
		{
			return;
		}

		Items[Index].bClientDeselected = true;

		Edit().Edit(Index);
	}
	
	void SetQuickSlotSelected(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId)
	{
		const int32 Index = IndexOf(BarId, QuickSlotId);
		if (Index == INDEX_NONE)
		{
			return;
		}

		Items[Index].bIsSelected = true;

		Edit().Edit(Index);
	}
	void SetQuickSlotDeselected(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId)
	{
		const int32 Index = IndexOf(BarId, QuickSlotId);
		if (Index == INDEX_NONE)
		{
			return;
		}

		Items[Index].bIsSelected = false;

		Edit().Edit(Index);
	}
	
	void SetItemSlot(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId, const FGameplayTag& ItemSlot)
	{
		const int32 Index = IndexOf(BarId, QuickSlotId);
		if (Index == INDEX_NONE)
		{
			return;
		}

		Items[Index].ItemSlot = ItemSlot;

		Edit().Edit(Index);
	}
	void SetAssignedItemId(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId, const FArcItemId& InItemId)
	{
		const int32 Index = IndexOf(BarId, QuickSlotId);
		if (Index == INDEX_NONE)
		{
			return;
		}
		Items[Index].OldAssignedItemId = Items[Index].AssignedItemId;
		Items[Index].AssignedItemId = InItemId;

		Edit().Edit(Index);
		Edit().MarkItemDirty(Items[Index]);
	}
	
	void RemoveItemSlot(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId)
    {
    	const int32 Index = IndexOf(BarId, QuickSlotId);
		if (Index == INDEX_NONE)
		{
			return;
		}
    
    	Items[Index].ItemSlot = FGameplayTag::EmptyTag;
    
    	Edit().Edit(Index);
    }
    	
	void RemoveAssignedItemId(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId)
	{
		const int32 Index = IndexOf(BarId, QuickSlotId);
		if (Index == INDEX_NONE)
		{
			return;
		}
		Items[Index].OldAssignedItemId = Items[Index].AssignedItemId;
		Items[Index].AssignedItemId = FArcItemId::InvalidId;

		Edit().Edit(Index);
		Edit().MarkItemDirty(Items[Index]);
	}
	TArray<FGameplayTag> GetAllActiveSlots(const FGameplayTag& InBarId) const
	{
		TArray<FGameplayTag> OutSlots;
		for (const FArcSelectedQuickBarSlot& QuickSlot : Items)
		{
			if (QuickSlot.BarId == InBarId)
			{
				if (QuickSlot.bIsSelected)
				{
					OutSlots.Add(QuickSlot.SlotId);	
				}
			}
		}
		return OutSlots;
	}

	FGameplayTag FindFirstSelectedSlot(const FGameplayTag& InBarId) const
	{
		for (const FArcSelectedQuickBarSlot& QuickSlot : Items)
		{
			if (QuickSlot.BarId == InBarId)
			{
				if (QuickSlot.bIsSelected)
				{
					return QuickSlot.SlotId;	
				}
			}
		}

		return FGameplayTag::EmptyTag;
	}
	
	int32 IndexOf(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId)
	{
		return Items.IndexOfByPredicate([BarId, QuickSlotId](const FArcSelectedQuickBarSlot& QuickSlotItem)
		{
			return QuickSlotItem.BarId == BarId && QuickSlotItem.SlotId == QuickSlotId;
		});
	}
	FArcSelectedQuickBarSlot* Find(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId)
	{
		return Items.FindByPredicate([BarId, QuickSlotId](const FArcSelectedQuickBarSlot& Slot)
		{
			return BarId == Slot.BarId && QuickSlotId == Slot.SlotId;
		});
	}

	const FArcSelectedQuickBarSlot* Find(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId) const
	{
		return Items.FindByPredicate([BarId, QuickSlotId](const FArcSelectedQuickBarSlot& Slot)
		{
			return BarId == Slot.BarId && QuickSlotId == Slot.SlotId;
		});
	}
	
	const FGameplayTag FindItemSlot(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId) const
	{
		const FArcSelectedQuickBarSlot* QuickBarSlot = Find(BarId, QuickSlotId);
		if (QuickBarSlot != nullptr)
		{
			return QuickBarSlot->ItemSlot;
		}

		return FGameplayTag::EmptyTag;
	}

	const FArcItemId FindItemId(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId) const
	{
		const FArcSelectedQuickBarSlot* QuickBarSlot = Find(BarId, QuickSlotId);
		if (QuickBarSlot != nullptr)
		{
			return QuickBarSlot->AssignedItemId;
		}

		return FArcItemId::InvalidId;
	}

	const FArcItemId FindOldItemId(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId) const
	{
		const FArcSelectedQuickBarSlot* QuickBarSlot = Find(BarId, QuickSlotId);
		if (QuickBarSlot != nullptr)
		{
			return QuickBarSlot->OldAssignedItemId;
		}

		return FArcItemId::InvalidId;
	}
	
	bool IsItemOnAnySlot(const FArcItemId& InItemId) const
	{
		for (const FArcSelectedQuickBarSlot& Item : Items)
		{
			if (Item.AssignedItemId == InItemId)
			{
				return true;
			}
		}

		return false;
	}

	// BarId, QuickSlotId
	TPair<FGameplayTag, FGameplayTag> FindItemBarAndSlot(const FArcItemId& InItemId) const
	{
		for (const FArcSelectedQuickBarSlot& Item : Items)
		{
			if (Item.AssignedItemId == InItemId)
			{
				return TPair<FGameplayTag, FGameplayTag>(Item.BarId, Item.SlotId);
			}
		}

		return TPair<FGameplayTag, FGameplayTag>();
	}
	
	const TArray<FArcSelectedQuickBarSlot>& GetSelections() const { return Items; }
 
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FArcSelectedQuickBarSlot, FArcSelectedQuickBarSlotList>(Items, DeltaParms, *this);
	}
};
 
template <>
struct TStructOpsTypeTraits<FArcSelectedQuickBarSlotList> : TStructOpsTypeTraitsBase2<FArcSelectedQuickBarSlotList>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};

UCLASS(BlueprintType)
class ARCCORE_API UArcQuickBarComponentBlueprint : public UBlueprint
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

USTRUCT()
struct FArcCycledSlotRPCData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int8 DeselectBarIdx = INDEX_NONE;

	UPROPERTY()
	int8 DeselectSlotIdx = INDEX_NONE;

	UPROPERTY()
	int8 OldBarIdx = INDEX_NONE;

	UPROPERTY()
	int8 OldSlotIdx = INDEX_NONE;

	UPROPERTY()
	int8 NewBarIdx = INDEX_NONE;

	UPROPERTY()
	int8 NewSlotIdx = INDEX_NONE;
	
	UPROPERTY()
	bool bUnbindInput = false;

	UPROPERTY()
	bool bBindInput = false;

	UPROPERTY()
	FGameplayTag SelectedItemSlot;

	UPROPERTY()
	FArcItemId SelectedItemId;
};

class UArcQuickBarComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FArcQuickBarComponentDelegate
	, UArcQuickBarComponent* /*QuickBarComponent*/
);

DECLARE_MULTICAST_DELEGATE_ThreeParams(FArcQuickBarDelegate
	, UArcQuickBarComponent* /*QuickBarComponent*/
	, FGameplayTag /*NewBarId*/
	, FGameplayTag /*OldBarId*/
);

DECLARE_MULTICAST_DELEGATE_FiveParams(FArcQuickBarSlotDelegate
	, UArcQuickBarComponent* /*QuickBarComponent*/
	, FGameplayTag /*NewBarId*/
	, FGameplayTag /*NewSlotId*/
	, FGameplayTag /*OldBarId*/
	, FGameplayTag /*OldSlotId*/
);

DECLARE_MULTICAST_DELEGATE_FourParams(FArcItemOnQuickSlot
	, UArcQuickBarComponent* /*QuickBarComponent*/
	, FGameplayTag /*NewBarId*/
	, FGameplayTag /*NewSlotId*/
	, FArcItemId /* ItemId */
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FArcItemOnQuickSlotDynamic
	, UArcQuickBarComponent*, QuickBarComponent
	, FGameplayTag, NewBarId
	, FGameplayTag, NewSlotId
	, FArcItemId, ItemId
);


UCLASS(BlueprintType)
class ARCCORE_API UArcQuickBarSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UArcQuickBarSubsystem* Get(UObject* WorldObject);
	
protected:
	DEFINE_ARC_CHANNEL_DELEGATE(AActor*, Actor, FArcQuickBarDelegate, OnQuickBarChanged);
	
	DEFINE_ARC_CHANNEL_DELEGATE(AActor*, Actor, FArcQuickBarSlotDelegate, OnQuickBarSlotChanged);

	DEFINE_ARC_CHANNEL_DELEGATE(AActor*, Actor, FArcItemOnQuickSlot, OnItemAddedToQuickSlot);

	DEFINE_ARC_CHANNEL_DELEGATE(AActor*, Actor, FArcItemOnQuickSlot, OnItemRemovedFromQuickSlot);
	
	DEFINE_ARC_CHANNEL_DELEGATE(AActor*, Actor, FArcQuickBarComponentDelegate, OnSlotCycleConfirmed)

public:
	UPROPERTY(BlueprintAssignable)
	FArcItemOnQuickSlotDynamic OnItemAddedToQuickSlot;

	UPROPERTY(BlueprintAssignable)
	FArcItemOnQuickSlotDynamic OnItemRemovedFromQuickSlot;

	UPROPERTY(BlueprintAssignable)
	FArcItemOnQuickSlotDynamic OnQuickBarSlotChanged;
};

/**
 * Quick Bar Component manages "Quick Bars" in generic way.
 * It allows to define multiple quick bars, which have multiple slots.
 * Component also implements basic way of "cycling" between slots in
 * quick bar (backward and forward) Cycling Quick Bars.
 *
 * Component also supports "binding" of input tags to items which are
 * placed on Slot.
 */
UCLASS(Blueprintable, ClassGroup = (Arc), meta = (BlueprintSpawnableComponent))
class ARCCORE_API UArcQuickBarComponent : public UActorComponent
{
	GENERATED_BODY()
	
	friend struct FArcSelectedQuickBarSlot;
	friend struct FArcSelectedQuickBarSlotList;
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arc Core")
	TArray<FArcQuickBar> QuickBars;

	UPROPERTY(Replicated)
	FArcSelectedQuickBarSlotList ReplicatedSelectedSlots;
	
	/**
	 * Hold delegates, while we wait for newly slotted item to be replicated back to clients.
	 * Right now only used @link AddItemToBarTrySelectOrRegisterDelegate
	 */
	TMap<FArcItemId, FDelegateHandle> WaitItemInitializeDelegateHandles;

	/** Indicates if cycling slot is locked ie. waiting for server to confirm changes. */
	int8 LockSlotCycle = 0;

	/** If some operation is replicated to server we lock any slot changes/selection until confirmation from server. */
	int8 LockQuickBar = 0;

public:
	void SetLockQuickBar()
	{
		LockQuickBar = 1;
	}
	void SetUnlockQuickBar()
	{
		LockQuickBar = 0;
	}
	
	bool IsQuickBarLocked() const
	{
		return LockQuickBar > 0;
	}
	/**
	 * @brief Tries To find SlotData by looking @ItemSlotMapping to find item Id, and then at assigned ItemSlotComponent.
	 * @param BarId Bar For SlotData
	 * @param QuickBarSlotId Slot On Bar 
	 * @return SlotData from bound ItemSlotComponent
	 */
	const FArcItemData* FindSlotData(const FGameplayTag& BarId
		, const FGameplayTag& QuickBarSlotId) const;

	// QuickBar, QuickSlot
	TPair<FGameplayTag, FGameplayTag> FindQuickSlotForItemId(const FArcItemId& InItemId) const
	{
		return ReplicatedSelectedSlots.FindItemBarAndSlot(InItemId);
	}
	
private:
	const FArcQuickBarSlot* FindQuickSlot(const FGameplayTag& BarId
		, const FGameplayTag& QuickBarSlotId) const;


public:
	// Sets default values for this component's properties
	UArcQuickBarComponent();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
	
	UFUNCTION(BlueprintPure, Category = "Arc Core")
	static UArcQuickBarComponent* GetQuickBar(class AActor* InOwner);

	UArcItemsStoreComponent* GetItemStoreComponent(const FArcQuickBar& InQuickBar) const;
	UArcItemsStoreComponent* GetItemStoreComponent(const FGameplayTag& InQuickBarId) const;
	UArcItemsStoreComponent* GetItemStoreComponent(const int32 InQuickBarId) const;

	bool IsItemOnAnyQuickSlot(const FArcItemId& InItemId) const
	{
		return ReplicatedSelectedSlots.IsItemOnAnySlot(InItemId);
	}
	
protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	/**
	 * We override it to register delegates from @link UGF1047ItemSlotComponent
	 * which will listen to Items added/removed from slot.
	 *
	 * In simplest case we will add item to @link ItemSlotMapping when it is added to slot.
	 * In More complex case we will also call @link FGF1047QuickBarInputBindHandling
	 *
	 * QuickSlots which will react to these delegates will not call @link FGF1047QuickBarSlot::SelectedHandlers
	 * I may change it later though. Right now these handlers are only called when Player decides
	 * to perform some action on this component.
	 */
	virtual void OnRegister() override;

	virtual void InitializeComponent() override;
	
	void HandleOnPlayerPawnReady(APawn* InPawn);

public:
	const TArray<FArcQuickBar>& GetQuickBars() const
	{
		return QuickBars;
	}

	/**
	 * Functions below should be used in context if this component.
	 * They all assume that you have valid item on ItemSlot (in @link UArcItemSlotComponent)
	 * from which those function can take @link FArcItemData and use it to Bind/Unbind inputs and call
	 * Handlers, when their state changes.
	 *
	 * You should not use those function, when you want to add new item to QuickSlot and ItemSlot at the same time.
	 */
	
	/**
	 * Cycles forward till it find slot which can be selected.
	 * @param BarId - bar which contains Slots we want to cycle.
	 * @param CurrentSlotId - Slot from which we start cycling
	 * @param SlotValidCondition - Custom function, which can check if slot is valid to cycle.
	 */
	FGameplayTag CycleSlotForward(const FGameplayTag& BarId
					  , const FGameplayTag& CurrentSlotId
					  , TFunction<bool(const FArcItemData*)> SlotValidCondition);

	/**
	 * Cycles backward till it find slot which can be selected.
	 * @param BarId - bar which contains Slots we want to cycle.
	 * @param CurrentSlotId - Slot from which we start cycling
	 * @param SlotValidCondition - Custom function, which can check if slot is valid to cycle.
	 */
	FGameplayTag CycleSlotBackward(const FGameplayTag& BarId
					  , const FGameplayTag& CurrentSlotId
					  , TFunction<bool(const FArcItemData*)> SlotValidCondition);
private:
	/**
	 * Check if we can cycle at all. If can return valid BarIdx.
	 */
	bool InternalCanCycle(const FGameplayTag& BarId
							  , int32& BarIdx);
	
	/**
	 * Function called when slot is selected from cycling.
	 */
	FGameplayTag InternalSelectCycledSlot(const FGameplayTag& BarId
										  , const int32 BarIdx
										  , const int32 OldSlotIdx
										  , UArcCoreAbilitySystemComponent* ArcASC
										  , const int32 NewSlotIdx);
	

	/**
	 * Helper function which loops over slots, and selects first one which meet conditions.
	 */
	bool InternalCycleSlot(const int32 StartSlotIdx
						  , const int32 BarIdx
						  , const int32 Direction
						  , TFunction<bool(const FArcItemData*)> SlotValidCondition
						  , int32& OutNewSlotIdx);
	
	/**
 	 * Helper function to look for @link FGF1047QuickBarInputBindHandling
 	 * and try to bind input. Return true if successful
 	 */
	bool InternalTryBindInput(const int32 BarIdx
		, const int32 QuickSlotIdx
		, UArcCoreAbilitySystemComponent* InArcASC);

	/**
	 * Helper function to look for @link FGF1047QuickBarInputBindHandling
	 * and try to unbind input. Return true if successful
	 */
	bool InternalTryUnbindInput(const int32 BarIdx
		, const int32 QuickSlotIdx
		, UArcCoreAbilitySystemComponent* InArcASC);
	
public:
	/**
 	 * @brief Activates all slots on specified bar. This means calling:
 	 * @link FArcQuickBarSlot#SelectedHandlers
 	 * And calling:
 	 * @link FArcQuickSlotHandler#OnSlotSelected
 	 * For each slot in bar.
 	 * Does not deactivate any activated slots.
 	 *
 	 * If Any of the slots on Bar does not have valid SlotData, bar activation fails.
 	 * You should make sure that this function is called after all needed data is already replicated.
 	 *
 	 * Slot is invalid, when there is no valid slotted item for that QuickSlot.
 	 * @param InBarId Bar to activate
 	 */
	void SelectBar(const FGameplayTag& InBarId);
	
private:
	/**
	 * Check all slots on Bar if they can have valid SlotData.
	 * If any of them doesn't have, bar is invalid.
	 */
	bool AreAllBarSlotsValid(const int32 BarIdx) const;

	/**
	 * Function to consolidate calling server into one RPC
	 */
	void ActivateSlotsOnBarInternal(const FGameplayTag& InBarId);
	
public:
	/**
	 * Deactivate all slots on bar. Calling all Handlers and unbinding inputs.
	 * Does not check if slots were earlier activated.
	 *
	 * It makes assumption that you are using it with @link SelectBar and not as
	 * another way to make sure you deactivate all slots.
	 */
	void DeselectBar(const FGameplayTag& InBarId);

private:
	/**
	 * Function to consolidate calling server into one RPC
	 */
	void DeactivateSlotsOnBarInternal(const FGameplayTag& InBarId);
	
public:
	/**
	 * Cycles Bar forward calling all events, 
	 */
	FGameplayTag CycleBarForward(const FGameplayTag& InOldBarId);

	UFUNCTION(BlueprintCallable, Category = "Arc Core")
	void SetBarSlotSelected(const FGameplayTag& InBarId
											, const FGameplayTag& InQuickSlotId
											, const bool bReplicate = true);
	
	UFUNCTION(Server, Reliable)
	void ServerSetBarSlotSelected(int8 BarIdx, int8 NewSlotIdx);
	
	/**
	 * Deactivate single specified Slot on ActionBar, calling all @link FArcQuickSlotHandler::OnSlotDeselected and bind Inputs.
	 *
	 * @param bReplicateToServer If making local change from Client, this parameter should be set to true, to make sure
	 * changes are replicated to server.
	 */
	UFUNCTION(BlueprintCallable, Category = "Arc Core")
	void SetBarSlotDeselected(const FGameplayTag& InBarId
							  , const FGameplayTag& InQuickSlotId
							  , const bool bReplicateToServer = true);

	UFUNCTION(Server, Reliable)
	void ServerSetBarSlotDeselected(int8 BarIdx, int8 NewSlotIdx);
	
	void InternalSetBarSlotDeselected(const FGameplayTag& InBarId
													  , const FGameplayTag& InQuickSlotId
													  , const bool bReplicateToServer);
	UFUNCTION(BlueprintPure, Category = "Arc Core")
	FGameplayTag GetFirstActiveSlot(const FGameplayTag& InBarId) const;
	
private:
	/**
	 * These functions should be used along with ItemSlot Component.
	 * If you have item, which is not currently slotted and it not on QuickSlot
	 * you can use @link AddItemToBarTrySelectOrRegisterDelegate To listen for item,
	 * till it get added to slot, and replicated back to clients.
	 *
	 * If item is already on ItemSlot, this function, will call the events (InputBinding and SlotSelected handlers).
	 * TODO: Might want to make calling those events optional (just indicate that something is on bar).
	 *
	 * When you want to remove item (either only from QuickSlot or QuickSlot and ItemSlot), you can use
	 * @link RemoveItemFromBar which will also correctly call all events. 
	 */
public:
	/**
	 * Adds Item to given Bar and QuickSlot.
	 *
	 * If SlotData is valid, it will InputBind that slot and call all selected handlers on it.
	 * If it is not valid, it will register delegate waiting for that item to be slotted and replicated back to client.
	 *
	 * You should only call this function on clients.
	 *
	 * This function does not call any RPCs.
	 * TODO: Maybe change name. It is somewhat verbose.
	 */
	void AddItemToBarOrRegisterDelegate(const FGameplayTag& InBarId
					  , const FGameplayTag& InQuickSlotId
					  , const FArcItemId& ItemId);

public:
	void RemoveItemFromBarByItemId(const FArcItemId& InItemId);
	
	/**
     * Remove Item from specific QuickSlot and Bar and unbind any inputs if slot data is still valid.
     * Call OnSlotDeselected.
     *
     * Does not replicate Back to Server.
     */
	void RemoveItemFromBar(const FGameplayTag& InBarId
						   , const FGameplayTag& InQuickSlotId);
protected:
	/**
	 * Helper function which is called when we added Item to bar trough @link AddItemToBarTrySelectOrRegisterDelegate
	 * and that Item was not yet on slot. In that case, we we will register into delegate waiting on ItemId
	 * till that item is replicated from SlotComponent, and then finish up setting it.
	 */
	virtual void HandleItemAddedFromReplication(UArcItemsStoreComponent* InItemsStore, const FArcItemData* InItem);

public:
	bool IsItemOnQuickSlot(const FGameplayTag& InBarId
						   , const FGameplayTag& InQuickSlotId) const
	{
		FArcItemId ItemId = ReplicatedSelectedSlots.FindItemId(InBarId, InQuickSlotId);
		
		return ItemId.IsValid();
	}
	
	TSubclassOf<UArcItemsStoreComponent> GetItemsStoreClass(const FArcQuickBar& InQuickBar) const
	{
		return InQuickBar.ItemsStoreClass;
	}

	TSubclassOf<UArcItemsStoreComponent> GetItemsStoreClass(const int32 QuickBarIdx) const
	{
		return QuickBars[QuickBarIdx].ItemsStoreClass;
	}

	TSubclassOf<UArcItemsStoreComponent> GetItemsStoreClass(const FGameplayTag& InBarId) const
	{
		int32 BarIdx = QuickBars.IndexOfByKey(InBarId);
		return QuickBars[BarIdx].ItemsStoreClass;
	}
	
	/**
	 * Server RPC used by functions which changes state of QuickSlot.
	 */
protected:
	UFUNCTION(Client, Reliable)
	void ClientUnlockQuickBar();
	
	/**
	 * Called when new slot have been selected from cycling slot on bar.
	 */
	UFUNCTION(Server, Reliable)
	void ServerHandleCycledSlot(const FArcCycledSlotRPCData& RPCData);

	UFUNCTION(Client, Reliable)
	void ClientConfirmSlotCycled();
	
	/**
	 * Called when new bar has been activated on client.
	 * It will perform Binding Inputs on slots and calling Handlers;
	 */
	UFUNCTION(Server, Reliable)
	void ServerSelectBar(int8 BarIdx);

	/**
	 * Called when bar has been deactivated on client.
	 * It will perform Binding Inputs on slots and calling Handlers; 
	 */
	UFUNCTION(Server, Reliable)
	void ServerDeselectBar(int8 BarIdx);

protected:
	virtual void HandleOnAbilityAdded(FGameplayAbilitySpec& AbilitySpec, UArcCoreAbilitySystemComponent* InASC);
	
	/**
	 * Called when new item is added to slot. It is only bound on non Dedicated Servers
	 * Will call Input binds and replicate back to server to also tell it to bind inputs,
	 *
	 * Does not call any handlers on slot.
	 *
	 * It is only called when @link FArcQuickBarSlot::ItemSlotId is set.
	 */
	virtual void HandleOnItemAddedToSlot(UArcItemsStoreComponent* InItemsStore
									 , const FArcItemData* InItem
									 , FGameplayTag BarId
									 , FGameplayTag InQuickSlotId);

	/**
	 * Called when item is removed from slot. It is only bound on non Dedicated Servers
	 * Will unbind any inputs and replicate back to server to also tell it to unbind as well.,
	 *
	 * Does not call any handlers on slot.
	 * 
	 * It is only called when @link FArcQuickBarSlot::ItemSlotId is set.
	 */
	virtual void HandleOnItemRemovedFromSlot(UArcItemsStoreComponent* InItemsStore
											 , const FArcItemData* InItem
											 , FGameplayTag BarId
											 , FGameplayTag InQuickSlotId);

	void HandlePendingItemsWhenSlotChanges(UArcItemsStoreComponent* ItemSlotComponent
											, const FGameplayTag& ItemSlot
											, const FArcItemData* ItemData);

	void HandlePendingItemsOnAbiilityGiven(FGameplayAbilitySpec& AbilitySpec, UArcCoreAbilitySystemComponent* InASC);
	
public:
	TArray<FGameplayTag> GetActiveSlots(const FGameplayTag& InBarId) const
	{
		return ReplicatedSelectedSlots.GetAllActiveSlots(InBarId);
	}


	UFUNCTION(BlueprintPure, Category = "Arc Core")
	FArcItemId GetItemId(const FGameplayTag& InBarId
						, const FGameplayTag& InQuickSlotId) const;

	UFUNCTION(BlueprintCallable, Category = "Arc Core")
	UArcItemsStoreComponent* BP_GetItemStoreComponent(const FGameplayTag& InQuickBarId) const;

	UFUNCTION(BlueprintCallable, Category = "Arc Core", Meta = (ExpandBoolAsExecs = "ReturnValue"))
	static bool BP_GetQuickBar(UArcQuickBarComponent* QuickBarComp, FGameplayTag InBarId, FArcQuickBar& QuickBar);

	UFUNCTION(BlueprintCallable, Category = "Arc Core", Meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool BP_GetItemFromSelectedSlot(FGameplayTag QuickBar, FArcItemDataHandle& OutItem);
};
