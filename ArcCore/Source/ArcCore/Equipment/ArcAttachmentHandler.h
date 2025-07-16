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

#include "Items/ArcItemId.h"
#include "Components/SceneComponent.h"
#include "StructUtils/InstancedStruct.h"
#include "Equipment/ArcSocketArray.h"
#include "Items/ArcItemDefinition.h"
#include "ArcAttachmentHandler.generated.h"

struct FGameplayTag;
struct FArcItemData;
struct FArcSlotData;
struct FArcItemAttachment;

class UArcItemAttachmentComponent;
class FName;

/**
 * Handlers define what happens when item is added to some slot and what happens
 * when item is being attached.
 */
USTRUCT()
struct ARCCORE_API FArcAttachmentHandler
{
	GENERATED_BODY()

public:
	virtual void Initialize(UArcItemAttachmentComponent* InAttachmentComponent) {};
	/**
	 * @brief Function is called to when item is added to slot, to decide what is
	 * happening with it. Like decide to which socket to attach and/or to which component attach.
	 * @param InAttachmentComponent Owning Item Attachment Component
	 * @param InItem Item Which granted this attachment
	 * @param SlotId Slot from which this attachment is coming from
	 * @param SocketName Optional socket name to which item should be attached, valid if item is attached from replication
	 * @return Item was successfully  attached
	 */
	virtual bool HandleItemAddedToSlot(UArcItemAttachmentComponent* InAttachmentComponent
									   , const FArcItemData* InItem
									   , const FArcItemData* InOwnerItem) const
	{
		return false;
	}

	virtual void HandleItemRemovedFromSlot(UArcItemAttachmentComponent* InAttachmentComponent
										   , const FArcItemData* InItem
										   , const FGameplayTag& SlotId
										   , const FArcItemData* InOwnerItem) const
	{
	};

	/**
	 * Override this function to decide how to attache item.
	 * At this point item should be already inside UArcItemAttachmentComponent::ReplicatedAttachments
	 * and you should be able to get it by using InItemId, and physically attach it in world.
	 *
	 * Remember to modify the corresponding FArcItemAttachment entry from ReplicatedAttachments to assign
	 * spawned Actor and/or SceneComponent.
	 */
	virtual void HandleItemAttach(UArcItemAttachmentComponent* InAttachmentComponent
								  , const FArcItemId InItemId
								  , const FArcItemId InOwnerItem) const {};

	virtual void HandleItemDetach(UArcItemAttachmentComponent* InAttachmentComponent
								  , const FArcItemId InItemId
								  , const FArcItemId InOwnerItem) const {};
	
	virtual void HandleItemAttachmentChanged(UArcItemAttachmentComponent* InAttachmentComponent
								  , const FArcItemAttachment& ItemAttachment) const {};

	
	virtual UScriptStruct* SupportedItemFragment() const
	{
		return nullptr;
	}
	
	virtual ~FArcAttachmentHandler() = default;
};

USTRUCT()
struct ARCCORE_API FArcAttachmentTransformFinder
{
	GENERATED_BODY()

	virtual FName FindSocketName(UArcItemAttachmentComponent* InAttachmentComponent, const FArcItemData* InItem, const FArcItemData* InOwnerItem) const
	{
		return NAME_None;
	}

	virtual FTransform FindRelativeTransform(UArcItemAttachmentComponent* InAttachmentComponent, const FArcItemData* InItem, const FArcItemData* InOwnerItem) const
	{
		return FTransform::Identity;
	}
	
	virtual ~FArcAttachmentTransformFinder() = default;
};

class UArcItemDefinition;
struct FArcItemFragment_ItemAttachmentSlots;

USTRUCT()
struct ARCCORE_API FArcAttachmentHandlerCommon : public FArcAttachmentHandler
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = "Base", meta = (TitleProperty = "Socket: {SocketName}"))
	TArray<FArcSocketArray> AttachmentData;

	UPROPERTY(EditAnywhere, Category = "Base")
	FName ComponentTag;

	UPROPERTY(EditAnywhere, Category = "Base", meta = (BaseStruct = "/Script/ArcCore.ArcAttachmentTransformFinder"))
	FInstancedStruct TransformFinder;

public:
	USceneComponent* FindAttachmentComponent(UArcItemAttachmentComponent* InAttachmentComponent, const FArcItemAttachment* ItemAttachment) const;

	FName FindSocketName(UArcItemAttachmentComponent* InAttachmentComponent, const FArcItemData* InItem, const FArcItemData* InOwnerItem) const;
	FTransform FindRelativeTransform(UArcItemAttachmentComponent* InAttachmentComponent, const FArcItemData* InItem, const FArcItemData* InOwnerItem) const;
	FArcItemAttachment MakeItemAttachment(UArcItemAttachmentComponent* InAttachmentComponent, const FArcItemData* InItem , const FArcItemData* InOwnerItem) const;

	FName FindFinalAttachSocket(const FArcItemAttachment* ItemAttachment) const;
	
	template<typename T>
	const T* FindAttachmentFragment(const FArcItemAttachment& ItemAttachment) const
	{
		if (ItemAttachment.VisualItemDefinition && ItemAttachment.VisualItemDefinition == ItemAttachment.OldVisualItemDefinition)
		{
			return nullptr;
		}
		
		if (ItemAttachment.VisualItemDefinition)
		{
			return ItemAttachment.VisualItemDefinition->FindFragment<T>();
		}

		if (ItemAttachment.ItemDefinition)
		{
			return ItemAttachment.ItemDefinition->FindFragment<T>();
		}

		return nullptr;
	}

	template<typename T = USceneComponent>
	T* SpawnComponent(UObject* Outer
		, UArcItemAttachmentComponent* InAttachmentComponent
		, const FArcItemAttachment* ItemAttachment
		, TSubclassOf<UActorComponent> ComponentClass = nullptr
		, const FName& Name = NAME_None) const
	{
		
		USceneComponent* ParentComponent = FindAttachmentComponent(InAttachmentComponent, ItemAttachment);

		if (ParentComponent == nullptr)
		{
			return nullptr;
		}

		UClass* ComponentClassToUse = ComponentClass ? ComponentClass.Get() : T::StaticClass();

		FName AttachSocketName = FindFinalAttachSocket(ItemAttachment);
		
		T* SpawnedComponent = NewObject<T>(Outer, ComponentClassToUse, Name);
		
		SpawnedComponent->SetCanEverAffectNavigation(false);
		SpawnedComponent->RegisterComponentWithWorld(Outer->GetWorld());
		SpawnedComponent->AttachToComponent(ParentComponent
			, FAttachmentTransformRules::SnapToTargetNotIncludingScale
			, AttachSocketName);

		return SpawnedComponent;
	}
	
	UArcItemDefinition* GetVisualItem(const FArcItemData* InItem) const;

	const FArcAttachmentHandlerCommon* FindAttachmentHandlerOnOwnerItem(const FArcItemData* InItem, const FArcItemData* InOwnerItem, UScriptStruct* InType) const;
	
};