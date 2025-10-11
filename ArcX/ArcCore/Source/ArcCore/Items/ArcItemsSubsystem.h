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

#include "ArcMacroDefines.h"
#include "Items/ArcItemId.h"

#include "GameplayTagContainer.h"
#include "Engine/GameInstance.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcItemsSubsystem.generated.h"

struct FArcItemData;
struct FArcItemAttachment;

class AActor;
class UArcItemsComponent;
class UArcItemsStoreComponent;
class UArcItemAttachmentComponent;

//DECLARE_MULTICAST_DELEGATE_TwoParams(FArcGenericItemsOperationState
//	, UArcItemsComponent* /* ItemsComponent */
//	, bool /* bOperationIsBlocked */);
//
//DECLARE_MULTICAST_DELEGATE_TwoParams(FArcGenericItemDelegate
//	, UArcItemsComponent*  /* ItemsComponent */
//	, const FArcItemData* /* ItemData */);

DECLARE_MULTICAST_DELEGATE_TwoParams(FArcGenericItemStoreDelegate
	, UArcItemsStoreComponent*  /* ItemsComponent */
	, const FArcItemData* /* ItemData */);

DECLARE_MULTICAST_DELEGATE_ThreeParams(FArcGenericItemSlotDelegate
	, UArcItemsStoreComponent* /* ItemSlotComponent */
	, const FGameplayTag& /* ItemSlot */
	, const FArcItemData* /* ItemData */);


DECLARE_MULTICAST_DELEGATE_FiveParams(FArcGenericItemSocketSlotDelegate
	, UArcItemsStoreComponent* /* ItemSlotComponent */
	, const FGameplayTag& /* ItemSlot */
	, const FArcItemData* /* OwnerItemData */
	, const FGameplayTag& /* ItemSocketSlot */
	, const FArcItemData* /* SocketItemData */);

DECLARE_MULTICAST_DELEGATE_ThreeParams(FArcGenericItemSlotOperationState
	, UArcItemsStoreComponent* /* ItemSlotComponent */
	, const FGameplayTag& /* ItemSlot */
	, const bool /* bOperationState */);

DECLARE_MULTICAST_DELEGATE_TwoParams(FArcGenericItemAttachmentDelegate
	, UArcItemAttachmentComponent*  /* ItemsAttachmentComponent */
	, const FArcItemAttachment* /* ItemAttachment */);

DECLARE_MULTICAST_DELEGATE_ThreeParams(FArcItemSlotState
	, UArcItemsStoreComponent* /* ItemsComponent */
	, const FGameplayTag& /* InSlot */
	, bool /* bOperationIsBlocked */);

DECLARE_MULTICAST_DELEGATE_FourParams(FArcItemAttachmentSlotState
	, UArcItemsStoreComponent* /* ItemsComponent */
	, const FArcItemId& /* InItemId */
	, const FGameplayTag& /* InSlot */
	, bool /* bOperationIsBlocked */);

DECLARE_MULTICAST_DELEGATE_ThreeParams(FArcItemStateDelegate
	, UArcItemsStoreComponent*  /* ItemsComponent */
	, const FArcItemId& /* ItemData */
	, bool /* bIsLocked */);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FArcGenericItemStoreDynamicDelegate
	, UArcItemsStoreComponent*, ItemsComponent
	, FArcItemId, ItemData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FArcGenericItemSlotDynamicDelegate
	, UArcItemsStoreComponent*, ItemSlotComponent
	, const FGameplayTag&, ItemSlot
	, FArcItemId, ItemId);

/**
 * 
 */
UCLASS()
class ARCCORE_API UArcItemsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	static UArcItemsSubsystem* Get(UObject* WorldObject);
	/**
	 * Items Store Delegates
	 */
	DEFINE_ARC_DELEGATE(FArcGenericItemStoreDelegate, OnItemAddedToStore);
	DEFINE_ARC_DELEGATE(FArcGenericItemStoreDelegate, OnItemRemovedFromStore);

	DEFINE_ARC_CHANNEL_DELEGATE(AActor*, Actor, FArcItemStateDelegate, OnItemStateChanged);
	DEFINE_ARC_CHANNEL_DELEGATE(AActor*, Actor, FArcItemSlotState, OnItemSlotStateChanged);
	DEFINE_ARC_CHANNEL_DELEGATE(AActor*, Actor, FArcItemAttachmentSlotState, OnItemAttachmentSlotStateChanged);
	
	DEFINE_ARC_CHANNEL_DELEGATE(AActor*, Actor, FArcGenericItemStoreDelegate, OnItemAddedToStore);
	DEFINE_ARC_CHANNEL_DELEGATE(AActor*, Actor, FArcGenericItemStoreDelegate, OnItemChangedStore);
	DEFINE_ARC_CHANNEL_DELEGATE(AActor*, Actor, FArcGenericItemStoreDelegate, OnItemRemovedFromStore);

	DEFINE_ARC_CHANNEL_DELEGATE_MAP(AActor*, Actor, FArcItemId, FArcGenericItemStoreDelegate, OnItemAddedToStore);
	DEFINE_ARC_CHANNEL_DELEGATE_MAP(AActor*, Actor, FArcItemId, FArcGenericItemStoreDelegate, OnItemChangedStore);
	DEFINE_ARC_CHANNEL_DELEGATE_MAP(AActor*, Actor, FArcItemId, FArcGenericItemStoreDelegate, OnItemRemovedFromStore);

	/**
	 * Item Slots delegates.
	 */
	DEFINE_ARC_CHANNEL_DELEGATE(AActor*, Actor, FArcGenericItemSlotDelegate, OnAddedToSlot);
	DEFINE_ARC_CHANNEL_DELEGATE(AActor*, Actor, FArcGenericItemSlotDelegate, OnSlotChanged);
	DEFINE_ARC_CHANNEL_DELEGATE(AActor*, Actor, FArcGenericItemSlotDelegate, OnRemovedFromSlot);

	DEFINE_ARC_CHANNEL_DELEGATE(AActor*, Actor, FArcGenericItemSocketSlotDelegate, OnAddedToSocket);
	DEFINE_ARC_CHANNEL_DELEGATE(AActor*, Actor, FArcGenericItemSocketSlotDelegate, OnRemovedSocket);
	
	DEFINE_ARC_CHANNEL_DELEGATE_MAP(AActor*, Actor, FGameplayTag, FArcGenericItemStoreDelegate, OnAddedToSlot);
	DEFINE_ARC_CHANNEL_DELEGATE_MAP(AActor*, Actor, FGameplayTag, FArcGenericItemStoreDelegate, OnRemovedFromSlot);
	
	DEFINE_ARC_CHANNEL_DELEGATE_MAP(AActor*, Actor, FArcItemId, FArcGenericItemStoreDelegate, OnItemAddedToSlot);
	DEFINE_ARC_CHANNEL_DELEGATE_MAP(AActor*, Actor, FArcItemId, FArcGenericItemStoreDelegate, OnItemSlotChanged);
	
	DEFINE_ARC_CHANNEL_DELEGATE(AActor*, Actor, FArcGenericItemAttachmentDelegate, OnItemAttached);
	DEFINE_ARC_CHANNEL_DELEGATE(AActor*, Actor, FArcGenericItemAttachmentDelegate, OnItemDetached);
	DEFINE_ARC_CHANNEL_DELEGATE(AActor*, Actor, FArcGenericItemAttachmentDelegate, OnItemAttachmentChanged);

public:
	UPROPERTY(BlueprintAssignable, Category = "Arc Core|Items")
	FArcGenericItemStoreDynamicDelegate OnItemChangedDynamic;
	
	UPROPERTY(BlueprintAssignable, Category = "Arc Core|Items")
	FArcGenericItemStoreDynamicDelegate OnItemAddedToStoreDynamic;

	UPROPERTY(BlueprintAssignable, Category = "Arc Core|Items")
	FArcGenericItemStoreDynamicDelegate OnItemRemovedFromStoreDynamic;

	UPROPERTY(BlueprintAssignable, Category = "Arc Core|Items")
	FArcGenericItemSlotDynamicDelegate OnItemAddedToSlotDynamic;

	UPROPERTY(BlueprintAssignable, Category = "Arc Core|Items")
	FArcGenericItemSlotDynamicDelegate OnItemRemovedFromSlotDynamic;
	
protected:
	/** Don't create this Subsystem for editor worlds. Only Game and PIE. */
	//virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
};
