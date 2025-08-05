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

#include "ArcCore/Items/ArcItemId.h"
#include "Components/ActorComponent.h"

#include "ArcNamedPrimaryAssetId.h"
#include "StructUtils/InstancedStruct.h"
#include "NativeGameplayTags.h"
#include "Iris/ReplicationState/IrisFastArraySerializer.h"
#include "Iris/ReplicationState/Private/IrisFastArraySerializerInternal.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "Net/Serialization/FastArraySerializer.h"

#include "Equipment/Fragments/ArcItemFragment_ItemAttachmentSlots.h"
#include "Items/ArcItemDefinition.h"
#include "Animation/AnimInstance.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ArcItemAttachmentComponent.generated.h"

class UAimOffsetBlendSpace;
class USkeletalMesh;
class UArcItemDefinition;
class ACharacter;
struct FArcItemData;
struct FArcItemAttachmentContainer;
class UArcItemAttachmentComponent;
class UArcItemsStoreComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogArcItemAttachment, Log, All);



USTRUCT()
struct FArcItemAttachmentContainer : public FIrisFastArraySerializer
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FArcItemAttachment> Items;

	UPROPERTY(NotReplicated)
	TObjectPtr<UArcItemAttachmentComponent> Owner = nullptr;

	mutable TMap<FArcItemId, FArcItemAttachment> ChangedItems;
	
#if UE_WITH_IRIS
	using ItemArrayType = TArray<FArcItemAttachment>;

	const ItemArrayType& GetItemArray() const
	{
		return Items;
	}

	ItemArrayType& GetItemArray()
	{
		return Items;
	}

	using FFastArrayEditor = UE::Net::TIrisFastArrayEditor<FArcItemAttachmentContainer>;

	FFastArrayEditor Edit()
	{
		return FFastArrayEditor(*this);
	}
#endif
	int32 GetItemIdx(FArcItemId InItem) const
	{
		for (int32 Idx = 0; Idx < Items.Num(); Idx++)
		{
			if (Items[Idx].ItemId == InItem)
			{
				return Idx;
			}
		}
		return INDEX_NONE;
	}

	int32 AddItemUnique(const FArcItemAttachment& InItem)
	{
		int32 Index = Items.IndexOfByPredicate([this, InItem](const FArcItemAttachment& Other)
		{
			return Other.ItemId == InItem.ItemId;
		});

		if (Index != INDEX_NONE)
		{
			return INDEX_NONE;
		}
		
		Edit().Add(InItem);
		MarkItemDirty(Items[Items.Num() - 1]);
		MarkArrayDirty();
		return Items.Num() - 1;
	}
	
	int32 AddItem(FArcItemId InItem
				  , FName SocketName
				  , const FGameplayTag& InSlot)
	{
		FArcItemAttachment I;
		I.ItemId = InItem;
		I.SocketName = SocketName;
		I.SlotId = InSlot;
		Edit().Add(I);
		return Items.Num() - 1;
	}

	void RemoveItem(FArcItemId InItem)
	{
		for (int32 Idx = 0; Idx < Items.Num(); Idx++)
		{
			if (Items[Idx].ItemId == InItem)
			{
				Items.RemoveAt(Idx);
				MarkArrayDirty();
				return;
			}
		}
	}

	void ItemChanged(const FArcItemId& InItem)
	{
		for (int32 Idx = 0; Idx < Items.Num(); Idx++)
		{
			if (Items[Idx].ItemId == InItem)
			{
				Items[Idx].ChangeUpdate++;
				Edit().Edit(Idx);
				MarkItemDirty(Items[Idx]);
				return;
			}
		}
	}

	template <typename Predicate>
	FORCEINLINE bool ContainsByPredicate(Predicate Pred) const
	{
		return Items.ContainsByPredicate(Pred);
	}

	//template <typename KeyType>
	//int32 IndexOfByKey(const KeyType& Key) const
	//{
	//	return Items.IndexOfByKey(Key);
	//}
	
	
	int32 IndexOfByKey(const FArcItemId& Key) const
	{
		for (int32 Idx = 0; Idx < Items.Num(); Idx++)
		{
			if (Items[Idx].ItemId == Key)
			{
				return Idx;
			}
		}
		return INDEX_NONE;
	}
	
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FArcItemAttachment, FArcItemAttachmentContainer>(Items
			, DeltaParms
			, *this);
	};

	bool Contains(const FArcItemId& Id) const
	{
		for (int32 Idx = 0; Idx < Items.Num(); Idx++)
		{
			if (Items[Idx].ItemId == Id)
			{
				return true;
			}
		}
		return false;
	}

	FArcItemAttachment& operator[](const int32 Index)
	{
		return Items[Index];
	}

	const FArcItemAttachment& operator[](const int32 Index) const
	{
		return Items[Index];
	}
	
	template <typename Predicate>
	int32 IndexOfByPredicate(Predicate Pred) const
	{
		return Items.IndexOfByPredicate(Pred);
	}

	TArray<FArcItemAttachment>::RangedForIteratorType begin() { return Items.begin(); }
	TArray<FArcItemAttachment>::RangedForConstIteratorType begin() const { return Items.begin(); }
	TArray<FArcItemAttachment>::RangedForIteratorType end() { return Items.end(); }
	TArray<FArcItemAttachment>::RangedForConstIteratorType end() const { return Items.end(); }
};

template <>
struct TStructOpsTypeTraits<FArcItemAttachmentContainer> : public TStructOpsTypeTraitsBase2<FArcItemAttachmentContainer>
{
	enum
	{
		WithNetDeltaSerializer = true
		, WithCopy = false
		,
	};
};



USTRUCT()
struct ARCCORE_API FArcItemAttachmentSlotContainer
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, meta = (TitleProperty = "QuickSlotId"))
	TArray<FArcItemAttachmentSlot> Slots;
};

UINTERFACE(NotBlueprintable)
class UArcAnimLayerItemInterface : public UInterface
{
	GENERATED_BODY()
};

class IArcAnimLayerItemInterface
{
	GENERATED_BODY()

public:
	virtual void SetSourceItemDefinition(const UArcItemDefinition* ItemDefinition) {};

	UFUNCTION(BlueprintCallable, Category = "Arc Core")
	virtual const UArcItemDefinition* GetSourceItemDefinition() const { return nullptr; }
};

UCLASS()
class ARCCORE_API UArcItemAnimInstance : public UAnimInstance, public IArcAnimLayerItemInterface
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<const UArcItemDefinition> SourceItemDefinition;

	virtual void SetSourceItemDefinition(const UArcItemDefinition* ItemDefinition) override
	{
		SourceItemDefinition = ItemDefinition;
	}

	virtual const UArcItemDefinition* GetSourceItemDefinition() const override
	{
		return SourceItemDefinition;
	}
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcLinkedAnimLayer
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<TSoftClassPtr<class UAnimInstance>> AnimLayers;

	UPROPERTY()
	TObjectPtr<const UArcItemDefinition> SourceItemDefinition;
	
	UPROPERTY()
	FArcItemId OwningItem;
};

class UAnimSequence;
class UBlendProfileStandalone;

USTRUCT(BlueprintType)
struct FArcItemFragment_ItemLayerCore : public FArcItemFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AssetBundles = "Game"))
	TSoftObjectPtr<UAnimSequence> ItemPose;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float ItemPoseBlendStrength = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float ItemPoseAdditiveAlpha = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float IKStrength = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AssetBundles = "Game"))
	TSoftObjectPtr<UAimOffsetBlendSpace> AimOffsetBlendSpace;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AssetBundles = "Game"))
	TSoftObjectPtr<UBlendProfileStandalone> BlendProfile;
};

class UArcItemAttachmentComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FArcItemAttachmentDynamicDelegate
	, UArcItemAttachmentComponent*, AttachmentComponent
	, const UArcItemDefinition*, ItemDefinition
	, const FGameplayTag&, SlotId);
/**
 * 
 */
UCLASS()
class ARCCORE_API UArcItemAttachmentSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FArcItemAttachmentDynamicDelegate OnItemAttachedDynamic;

	UPROPERTY(BlueprintAssignable)
	FArcItemAttachmentDynamicDelegate OnItemDetachedDynamic;
};

ARCCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Invalid_Item_Slot);

/*
 * Manages how item representation (be it component or actor) are attached and where.
 * Handle replication (attachment are by default not created on dedicated servers).
 */
UCLASS(ClassGroup = (Arc), meta = (BlueprintSpawnableComponent), Blueprintable)
class ARCCORE_API UArcItemAttachmentComponent : public UActorComponent
{
	GENERATED_BODY()
	friend struct FArcItemAttachment;

private:
	TWeakObjectPtr<APawn> CurrentPawn;

	/**
	 * List of replicated attachments.
	 */
	UPROPERTY(Replicated)
	FArcItemAttachmentContainer ReplicatedAttachments;

	UPROPERTY(ReplicatedUsing = OnRep_LinkedAnimLayer)
	FArcLinkedAnimLayer LinkedAnimLayer;

	/**
	 * Will listen for slotted items from selected @link UArcItemsStoreComponent class.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Arc Core")
	TSubclassOf<UArcItemsStoreComponent> ItemSlotClass;

	/**
	 * This is list of Slots which are automatically handled by this component, when items are added to Slot in @link UArcItemsStoreComponent
	 * Each entry has @link FArcAttachmentHandler which are used in order to try handle attachment from slotted Item.
	 * There is some default implementations, to handle the simple generic cases.
	 *
	 * Please do not that attachment do not handle in any way situations where multiple items are attached one slot.
	 * What happen in that case you need to decide on your own. It is left that way, because you may want to define
	 * slots for "sub" attachments like scope to weapon. In that case in stead of creating gameplay tag slot for each
	 * possible weapon/slot combination you can have one, Weapon.Gun.Scope with handlers handling attaching scope.
	 *
	 * What matters in this case is how is owner of Scope, not the config define there.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Arc Core")
	FArcItemAttachmentSlotContainer StaticAttachmentSlots;

	/**
	 * Im not sure if we want or not to save it.
	 * Might want to save to know at which sockets items are attached ?
	 */
	//TArray<FArcAttachmentContainer> AttachedItems;

	/** Attachments pending for their owner to replicate. OwnerId, Attachment Id */
	TMap<FArcItemId, FArcItemId> PendingAttachments;

	/**
	 * List of taken sockets.
	 * Key is Slot to which item Attached.
	 * Value is set of Taken sockets on Slot, which is granting them.
	 */
	TMap<FGameplayTag, TSet<FName>> TakenSockets;

	TMap<const UArcItemDefinition*, TArray<UObject*>> ObjectsAttachedFromItem;



public:
	const FArcItemAttachmentSlotContainer& GetStaticAttachmentSlots() const
	{
		return StaticAttachmentSlots;
	}

	const FArcItemAttachmentContainer& GetReplicatedAttachments() const
	{
		return ReplicatedAttachments;
	}
	
	const TMap<FArcItemId, FArcItemId>& GetPendingAttachments() const
	{
		return PendingAttachments;
	}

	UClass* GetItemStoreClass() const;
#if WITH_EDITORONLY_DATA
	/** This scans the class for AssetBundles metadata on asset properties and initializes the AssetBundleData with InitializeAssetBundlesFromMetadata */
	virtual void UpdateAssetBundleData();

	/** Asset Bundle data computed at save time. In cooked builds this is accessible from AssetRegistry */
	UPROPERTY()
	FAssetBundleData AssetBundleData;
#endif
	
	UObject* FindFirstAttachedObject(const UArcItemDefinition* InItemDefinition) const
	{
		if (const TArray<UObject*>* Objects = ObjectsAttachedFromItem.Find(InItemDefinition))
		{
			if (Objects->Num() > 0)
			{
				return (*Objects)[0];
			}
		}

		return nullptr;
	}

	TArray<UObject*> FindAttachedObjects(const UArcItemDefinition* InItemDefinition) const
	{
		if (const TArray<UObject*>* Objects = ObjectsAttachedFromItem.Find(InItemDefinition))
		{
			return (*Objects);
		}

		return TArray<UObject*>();
	}

	template<typename T>
	TArray<T*> FindAttachedObjectsOfType(const UArcItemDefinition* InItemDefinition) const
	{
		TArray<T*> Found;
		Found.Reserve(8);
		
		if (const TArray<UObject*>* Objects = ObjectsAttachedFromItem.Find(InItemDefinition))
		{
			for (UObject* Obj : (*Objects))
			{
				if (Obj && Obj->GetClass()->IsChildOf(T::StaticClass()))
				{
					Found.Add(Cast<T>(Obj));
				}
			}
		}

		return Found;
	}

	
	TArray<UObject*> FindAttachedObjectsOfClass(const UArcItemDefinition* InItemDefinition, UClass* InClass) const
	{
		TArray<UObject*> Found;
		Found.Reserve(8);
		
		if (const TArray<UObject*>* Objects = ObjectsAttachedFromItem.Find(InItemDefinition))
		{
			for (UObject* Obj : (*Objects))
			{
				if (Obj && Obj->GetClass()->IsChildOf(InClass))
				{
					Found.Add(Obj);
				}
			}
		}

		return Found;
	}
	
	
	TArray<USkeletalMeshComponent*> FindAttachedSkeletalMeshes(const UArcItemDefinition* InItemDefinition, USkeletalMesh* InSkeletalMesh) const;

	template<typename T>
	T* FindFirstAttachedObject(const UArcItemDefinition* InItemDefinition) const
	{
		return Cast<T>(FindFirstAttachedObject(InItemDefinition));
	}
	
	void AddAttachmentForItem(const UArcItemDefinition* InItemDef, UObject* InAttachedObject)
	{
		ObjectsAttachedFromItem.FindOrAdd(InItemDef).Add(InAttachedObject);
	}

	void RemoveAttachedForItem(const UArcItemDefinition* InItemDefinition)
	{
		ObjectsAttachedFromItem.Remove(InItemDefinition);
	}
	
public:
	bool IsSocketTaken(const FGameplayTag& InSlot, const FName& InSocketName) const;

	void RemovePendingAttachment(const FArcItemId InOwnerItem)
	{
		PendingAttachments.Remove(InOwnerItem);
	}

	void AddPendingAttachment(const FArcItemId InOwnerId, const FArcItemId ChildItemId)
	{
		PendingAttachments.FindOrAdd(InOwnerId, ChildItemId);
	}
	
	bool DoesSlotHaveAttachment(const FGameplayTag& InSlot) const
	{
		return ReplicatedAttachments.ContainsByPredicate([InSlot, this] (const FArcItemAttachment& Item)
		{
			return Item.SlotId == InSlot;
		});
	}

	bool DoesSlotHaveAttachedActor(const FGameplayTag& InSlot) const;

public:
	ACharacter* FindCharacter() const;

	void SetVisualItemAttachment(UArcItemDefinition* InItemDefinition, const FArcItemId& ForItem);
	void ResetVisualItemAttachment(const FArcItemId& ForItem);
	/**
	 * Avoid using directly. It is used with @link FArcAttachmentHandler To add Attachment to this component.
	 */
	void AddAttachedItem(const FArcItemAttachment& InAttachment);

	void AddAttachedItem(const FArcItemData* InItem
		, const FArcItemData* InOwnerItem
		, const FName& AttachSocket
		, UArcItemDefinition* InVisualItem);
	
	bool ContainsAttachedItem(const FArcItemId& InItemId) const
	{
		if (ReplicatedAttachments.ContainsByPredicate([InItemId] (const FArcItemAttachment& InItemPred)
		{
			if (InItemPred.ItemId == InItemId)
			{
				return true;
			}
			return false;
		}) == false)
		{
			return false;
		}

		return true;
	}

	const FArcItemAttachment* GetAttachment(const FArcItemId& InItemId) const
	{
		int32 Idx = ReplicatedAttachments.IndexOfByKey(InItemId);
		if (Idx != INDEX_NONE)
		{
			return &ReplicatedAttachments.Items[Idx];
		}
		return nullptr;
	}

	void RemoveAttachment(const FArcItemId& InItemId)
	{
		int32 Idx = ReplicatedAttachments.IndexOfByKey(InItemId);
		if (Idx != INDEX_NONE)
		{
			if (TSet<FName>* Set = TakenSockets.Find(ReplicatedAttachments.Items[Idx].SlotId))
			{
				Set->Remove(ReplicatedAttachments.Items[Idx].SocketName);
			}
			
			ReplicatedAttachments.RemoveItem(InItemId);
		}
	}

public:
	// Sets default values for this component's properties
	UArcItemAttachmentComponent();

	UFUNCTION(BlueprintPure, Category = "Arc Core")
	static UArcItemAttachmentComponent* FindItemAttachmentComponent(class AActor* InActor);

protected:
	virtual void OnRegister() override;
	
	virtual void InitializeComponent() override;
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	FName GetItemSocket(FArcItemId InItemId) const;

	/**
 * @brief Retrieves the attached actor corresponding to the given item ID.
 *
 * @param InItemId The ID of the item to find the attached actor for.
 *
 * @return A pointer to the attached actor, or nullptr if no actor is found.
 */
	AActor* GetAttachedActor(FArcItemId InItemId) const;

	
	/**
	 * @brief TODO: Right now it just directly attach AttachedActor from Item to specific Socket on Character.
	 * without changing the sockets and/or slots to which item is attached.
	 * The sample use is when we have Weapon to unholster and want to attach it to hand.
	 * @param InItem 
	 * @param InSocketName 
	 */
	void AttachItemToSocket(const FArcItemId& InItem
							, FName InSocketName
							, FName SceneComponentTag
							, const FTransform& RelativeTransform = FTransform::Identity);

	void DetachItemFromSocket(const FArcItemId& InItem);

	/**
	 * @brief Links the specified animation layers to the character's mesh anim instance.
	 *
	 * This function is called to link the animation layers of a specific item to the character's mesh anim instance.
	 * It performs the following steps:
	 * 1. Retrieves the player state from the owner of the component.
	 * 2. Retrieves the character pawn from the player state.
	 * 3. Retrieves the arc items store component from the player state.
	 * 4. Retrieves the item data for the specified slot from the arc items store component.
	 * 5. Retrieves the animation layer fragment from the item data.
	 * 6. Links each animation layer in the fragment to the character's mesh anim instance.
	 * 7. Updates the linked animation layers in the component's LinkedAnimLayer property.
	 * 8. Marks the LinkedAnimLayer property as dirty.
	 *
	 * @param InSlot The gameplay tag of the item slot to link the animation layers for.
	 */
	void LinkAnimLayer(const FGameplayTag& InSlot);
	void LinkAnimLayer(const FArcItemId& InItemId);
	
	void UnlinkAnimLayer(const FGameplayTag& InSlot);

public:
	void CallbackItemAttached(const FArcItemId InItemId);
	
protected:
	void HandleItemAddedFromReplication(const FArcItemId InItemId);

	void HandleItemRemovedFromReplication(const FArcItemId InItemId);

	void HandleItemChangedFromReplication(const FArcItemId InItemId);

	void HandleAttachSocketChanged(const FArcItemId& InItemId);
	
	virtual void HandleItemAddedToSlot(UArcItemsStoreComponent* InItemSlots
									   , const FGameplayTag& SlotId
									   , const FArcItemData* Item);

	void HandleOnItemAttachedToSocket(UArcItemsStoreComponent* ItemSlotComponent
								  , const FGameplayTag& ItemSlot
								  , const FArcItemData* OwnerItemData
								  , const FGameplayTag& ItemSocketSlot
								  , const FArcItemData* SocketItemData);
	
	virtual void HandleItemRemovedFromSlot(UArcItemsStoreComponent* InItemSlots
										   , const FGameplayTag& SlotId
										   , const FArcItemData* Item);


	
	UFUNCTION()
	virtual void OnRep_LinkedAnimLayer(const FArcLinkedAnimLayer& OldLinkedLayer);

	void HandleOnPlayerPawnReady(class APawn* InPawn);

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Attachment")
	static const UArcItemDefinition* GetItemDefinitionFromAttachment(AActor* Owner, FGameplayTag SlotId);
	
};
