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

#include "GameplayTagContainer.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcNamedPrimaryAssetId.h"
#include "Items/ArcItemId.h"

#include "ArcSocketArray.generated.h"

USTRUCT(BlueprintType)
struct ARCCORE_API FArcSocketArray
{
	GENERATED_BODY()

public:
	/**
	 * Item must have these tagsl
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FGameplayTagContainer RequiredTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FGameplayTagContainer SocketTags;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName SocketName;
};

USTRUCT()
struct ARCCORE_API FArcItemAttachmentSlot
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Data", meta = (NoCategoryGrouping, Categories = "SlotId"))
	FGameplayTag SlotId;

	UPROPERTY(EditAnywhere, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId DefaultVisualItem;
	
	/*
	 * All of those will be executed if something is added to this slot.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Arc Core", meta = (BaseStruct = "/Script/ArcCore.ArcAttachmentHandler"))
	TArray<FInstancedStruct> Handlers;
};

/**
 * Custom data which can be attached to @link FArcItemAttachment
 * To add some custom data associated with attachment. Like custom Material Parameters, scale etc.
 */
USTRUCT()
struct ARCCORE_API FArcItemAttachmentCustomData
{
	GENERATED_BODY()

	virtual ~FArcItemAttachmentCustomData() = default;
};

class UArcItemDefinition;
struct FArcItemAttachmentContainer;

USTRUCT()
struct ARCCORE_API FArcItemAttachment : public FFastArraySerializerItem
{
	GENERATED_BODY()

public:
	/** Id of main item from which this attachment has been created. */
	UPROPERTY(SaveGame)
	FArcItemId ItemId;

	/** Id of owning item, if this attachment is from item which is socketed into another item. */
	UPROPERTY(SaveGame)
	FArcItemId OwnerId;

	/** Default socket to which item is attached. */
	UPROPERTY(SaveGame)
	FName SocketName = NAME_None;

	UPROPERTY(SaveGame)
	FName SocketComponentTag = NAME_None;
	
	/** Socket to which we can attach item later. Ie, unholster weapon on server. */
	UPROPERTY(SaveGame)
	FName ChangedSocket = NAME_None;

	UPROPERTY(SaveGame)
	FName ChangeSceneComponentTag = NAME_None;;
	
	/** Id of slot which created this attachment. */
	UPROPERTY(SaveGame)
	FGameplayTag SlotId;

	/** Owning slot if this attachment is for socket item.*/
	UPROPERTY(SaveGame)
	FGameplayTag OwnerSlotId;

	UPROPERTY(SaveGame)
	FTransform RelativeTransform;
	
	UPROPERTY()
	uint8 ChangeUpdate;

	UPROPERTY()
	TObjectPtr<const UArcItemDefinition> VisualItemDefinition  = nullptr;

	UPROPERTY()
	TObjectPtr<const UArcItemDefinition> OldVisualItemDefinition  = nullptr;
	
	UPROPERTY()
	TObjectPtr<const UArcItemDefinition> ItemDefinition  = nullptr;

	UPROPERTY()
	TObjectPtr<const UArcItemDefinition> OwnerItemDefinition  = nullptr;

	// TODO:: It doesn't do anything:
	// TODO:: Make it array.
	// TODO:: Replace all anove properties except ItemId and OwnerId with this.
	TSharedPtr<FArcItemAttachmentCustomData> CustomData;

	const FArcItemAttachmentContainer* OwnerArray = nullptr;
	
	FArcItemAttachment()
		: ItemId()
		, OwnerId()
		, SocketName(NAME_None)
		, ChangedSocket(NAME_None)
		, SlotId()
		, OwnerSlotId()
		, ChangeUpdate(0)
	{
	}

	FArcItemAttachment(const FArcItemAttachment& Other)
	{
		OwnerArray = Other.OwnerArray;
		ItemId = Other.ItemId;
		OwnerId = Other.OwnerId;
		SocketName = Other.SocketName;
		ChangedSocket = Other.ChangedSocket;
		SlotId = Other.SlotId;
		OwnerSlotId = Other.OwnerSlotId;
		RelativeTransform = Other.RelativeTransform;
		ChangeUpdate = Other.ChangeUpdate;
		VisualItemDefinition = Other.VisualItemDefinition;
		OldVisualItemDefinition = Other.OldVisualItemDefinition;
		ItemDefinition = Other.ItemDefinition;
		OwnerItemDefinition = Other.OwnerItemDefinition;;
	}
	
	void PreReplicatedRemove(const FArcItemAttachmentContainer& InArraySerializer);

	void PostReplicatedAdd(const FArcItemAttachmentContainer& InArraySerializer);

	void PostReplicatedChange(const FArcItemAttachmentContainer& InArraySerializer);

	FArcItemAttachment& operator=(const FArcItemAttachment& Other)
	{
		OwnerArray = Other.OwnerArray;
		ItemId = Other.ItemId;
		OwnerId = Other.OwnerId;
		SocketName = Other.SocketName;
		ChangedSocket = Other.ChangedSocket;
		SlotId = Other.SlotId;
		OwnerSlotId = Other.OwnerSlotId;
		RelativeTransform = Other.RelativeTransform;
		ChangeUpdate = Other.ChangeUpdate;
		VisualItemDefinition = Other.VisualItemDefinition;
		OldVisualItemDefinition = Other.OldVisualItemDefinition;
		ItemDefinition = Other.ItemDefinition;
		OwnerItemDefinition = Other.OwnerItemDefinition;
		//CustomData = Other.CustomData;

		return *this;
	}
	
	bool operator==(const FArcItemAttachment& Other) const;

	bool operator!=(const FArcItemAttachment& Other) const
	{
		return ItemId != Other.ItemId
			|| OwnerId != Other.OwnerId
			|| SocketName != Other.SocketName
			|| ChangedSocket != Other.ChangedSocket
			|| SlotId != Other.SlotId
			|| OwnerSlotId != Other.OwnerSlotId
			//|| RelativeTransform != Other.RelativeTransform
			|| ChangeUpdate != Other.ChangeUpdate
			|| VisualItemDefinition != Other.VisualItemDefinition
			|| OldVisualItemDefinition != Other.OldVisualItemDefinition
			|| ItemDefinition != Other.ItemDefinition
			|| OwnerItemDefinition != Other.OwnerItemDefinition;
			//|| Other.CustomData 1= Other.CustomData;
	}
};
template <>
struct TStructOpsTypeTraits<FArcItemAttachment>
		: public TStructOpsTypeTraitsBase2<FArcItemAttachment>
{
	enum
	{
		WithCopy = true
		, WithIdenticalViaEquality = true
		, // We have a custom compare operator
	};
};