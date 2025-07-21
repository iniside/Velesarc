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


#include "StructUtils/InstancedStruct.h"
#include "Components/GameFrameworkComponent.h"
#include "Engine/DataAsset.h"
#include "Items/ArcItemData.h"
#include "ArcEquipmentComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogArcEquipmentComponent, Log, All);

class UArcEquipmentSlotPreset;
class UArcItemsStoreComponent;

/**
 * \class UArcEquipmentComponent
 *
 * \brief A class that represents an equipment component for the Arc game.
 *
 * This class is a blueprint spawnable component that extends the UArcItemsComponent class.
 * It provides functionality for managing and interacting with equipment slots.
 */
UCLASS(ClassGroup=(Arc), meta=(BlueprintSpawnableComponent, LoadBehavior = "Eager"))
class ARCCORE_API UArcEquipmentComponent : public UGameFrameworkComponent
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere)
	TObjectPtr<UArcEquipmentSlotPreset> EquipmentSlotPreset;

	UPROPERTY(EditAnywhere)
	TSubclassOf<UArcItemsStoreComponent> ItemsStoreClass;

	UPROPERTY()
	mutable TObjectPtr<UArcItemsStoreComponent> ItemsStore;
	
	UPROPERTY(Transient, SaveGame)
	TMap<FGameplayTag, FArcItemId> EquippedItems;
	
public:
	// Sets default values for this component's properties
	UArcEquipmentComponent(const FObjectInitializer& ObjectInitializer);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	UArcItemsStoreComponent* GetItemsStore() const;

protected:
	bool CanEquipItem(const UArcItemDefinition* InItemDefinition, const FArcItemData* InItemData, const FGameplayTag& InSlotId) const;
	
public:
	bool CanEquipItem(const FArcItemId& InItemId, UArcItemsStoreComponent* ItemStore, const FGameplayTag& InSlotId) const;

	bool CanEquipItem(const FPrimaryAssetId& ItemDefinitionId, const FGameplayTag& InSlotId) const;
	
	void GetTagsForSlot(const FGameplayTag& InSlotId, FGameplayTagContainer& OutRequiredTags, FGameplayTagContainer& OutIgnoreTags) const;

	void EquipItem(UArcItemsStoreComponent* FromItemsStore, const FArcItemId& InNewItem, const FGameplayTag& ToSlot);

	TArray<const FArcItemData*> GetItemsFromStoreForSlot(const FGameplayTag& InSlotId) const;
	
	TArray<FArcEquipmentSlot> GetMatchingSlots(const FGameplayTag& InTag) const;

	const TArray<FArcEquipmentSlot>& GetEquipmentSlots() const;
};

USTRUCT()
struct ARCCORE_API FArcEquipmentSlotCondition
{
	GENERATED_BODY()

	bool CanEquip(AActor* OwnerActor, const UArcItemDefinition* InItemDefinition, const FArcItemData* InItemData) const { return true; }
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcEquipmentSlot
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "SlotId"))
	FGameplayTag SlotId;

	/* If set item must have all of those tags. */
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer RequiredTags;

	/* If set item must have none of those tags. */
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer IgnoreTags;

	/* If set Owner must have all of those tags. */
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer OwnerRequiredTags;

	/* If set Owner must have none of those tags. */
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer OwnerIgnoreTags;
	
	/* Custom implemented condition which must be meet for item to be fit into this slot. */
	UPROPERTY(EditAnywhere, meta = (BaseStruct = "/Script/ArcCore.ArcEquipmentSlotCondition"))
	FInstancedStruct CustomCondition;
};

UCLASS()
class ARCCORE_API UArcEquipmentSlotPreset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	TArray<FArcEquipmentSlot> EquipmentSlots;
};

USTRUCT()
struct FArcEquipmentSlotDefaultItem
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, meta = (Categories = "QuickSlotId"))
	FGameplayTag SlotId;

	/* If set item must have all of those tags. */
	UPROPERTY(EditAnywhere, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId ItemId;
};

UCLASS()
class ARCCORE_API UArcEquipmentSlotDefaultItems : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	TArray<FArcEquipmentSlotDefaultItem> EquipmentSlots;

	void AddItemsToEquipment(UArcEquipmentComponent* InEquipmentComponent);
};