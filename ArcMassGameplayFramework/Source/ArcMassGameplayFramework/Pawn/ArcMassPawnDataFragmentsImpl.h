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

#include "CoreMinimal.h"
#include "ArcNamedPrimaryAssetId.h"
#include "GameplayTagContainer.h"
#include "Pawn/ArcMassPawnDataFragments.h"
#include "UObject/SoftObjectPtr.h"

#include "ArcMassPawnDataFragmentsImpl.generated.h"

class UActorComponent;
class UAnimInstance;
class USkeletalMesh;
class UArcMassAbilitySet;
class UArcMassPawnExtensionComponent;

/**
 * FArcMassAddPawnComponent_Single
 *
 * Spawns and registers a single UActorComponent on the pawn during DataInitialized.
 * Server/client filter is applied via bServerOnly / bClientOnly.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Add Pawn Component (Single)"))
struct ARCMASSGAMEPLAYFRAMEWORK_API FArcMassAddPawnComponent_Single : public FArcMassAddPawnComponent
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = "Data", meta = (AssetBundles = "Client"))
	TSoftClassPtr<UActorComponent> ComponentClass;

	UPROPERTY(EditAnywhere, Category = "Data")
	bool bReplicate = true;

	UPROPERTY(EditAnywhere, Category = "Data")
	bool bServerOnly = false;

	UPROPERTY(EditAnywhere, Category = "Data")
	bool bClientOnly = false;

public:
	virtual void GiveComponent(APawn* Pawn) const override;

	virtual ~FArcMassAddPawnComponent_Single() override = default;
};

/**
 * FArcMassPawnInitFragment_SetSkeletalMesh
 *
 * Assigns a USkeletalMesh to the pawn's character mesh component.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Set Skeletal Mesh"))
struct ARCMASSGAMEPLAYFRAMEWORK_API FArcMassPawnInitFragment_SetSkeletalMesh : public FArcMassPawnInitFragment
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = "Data", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

public:
	virtual void Initialize(APawn* Pawn
							, APlayerState* PlayerState
							, APlayerController* PlayerController
							, FMassEntityManager* EntityManager
							, FMassEntityHandle Entity
							, UArcMassPawnExtensionComponent* OwnerExtension) const override;

	virtual ~FArcMassPawnInitFragment_SetSkeletalMesh() override = default;
};

/**
 * FArcMassPawnInitFragment_SetAnimBlueprint
 *
 * Sets the anim instance class on the pawn's character mesh component, and links
 * any configured anim layer classes.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Set Anim Blueprint"))
struct ARCMASSGAMEPLAYFRAMEWORK_API FArcMassPawnInitFragment_SetAnimBlueprint : public FArcMassPawnInitFragment
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = "Data", meta = (AssetBundles = "Client"))
	TSoftClassPtr<UAnimInstance> AnimBlueprint;

	UPROPERTY(EditAnywhere, Category = "Data", meta = (AssetBundles = "Client"))
	TArray<TSoftClassPtr<UAnimInstance>> AnimLayers;

	/** Tag used to select which USkeletalMeshComponent on the pawn receives the anim blueprint.
	 *  If None, falls back to ACharacter::GetMesh() or the first skeletal mesh found. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName MeshComponentTag;

public:
	virtual void Initialize(APawn* Pawn
							, APlayerState* PlayerState
							, APlayerController* PlayerController
							, FMassEntityManager* EntityManager
							, FMassEntityHandle Entity
							, UArcMassPawnExtensionComponent* OwnerExtension) const override;

	virtual ~FArcMassPawnInitFragment_SetAnimBlueprint() override = default;
};

/**
 * FArcMassPawnInitFragment_GrantAbilitySet
 *
 * Grants a UArcMassAbilitySet to the pawn's Mass entity during DataInitialized.
 * Server-authority only. Silently skipped for non-Mass pawns.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Grant Mass Ability Set"))
struct ARCMASSGAMEPLAYFRAMEWORK_API FArcMassPawnInitFragment_GrantAbilitySet : public FArcMassPawnInitFragment
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = "Data")
	TObjectPtr<UArcMassAbilitySet> AbilitySet;

public:
	virtual void Initialize(APawn* Pawn
							, APlayerState* PlayerState
							, APlayerController* PlayerController
							, FMassEntityManager* EntityManager
							, FMassEntityHandle Entity
							, UArcMassPawnExtensionComponent* OwnerExtension) const override;

	virtual ~FArcMassPawnInitFragment_GrantAbilitySet() override = default;
};

USTRUCT(BlueprintType)
struct ARCMASSGAMEPLAYFRAMEWORK_API FArcMassPawnAddItemEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Data", meta = (AllowedTypes = "ArcItem"))
	FArcNamedPrimaryAssetId Item;

	UPROPERTY(EditAnywhere, Category = "Data")
	uint8 Level = 1;

	UPROPERTY(EditAnywhere, Category = "Data")
	int32 Amount = 1;
};

/**
 * FArcMassPawnInitFragment_AddItems
 *
 * Adds a list of items to a Mass item store on the pawn's entity during DataInitialized.
 * StoreType selects which FArcMassItemStoreFragment-derived store receives the items.
 * Server-authority only.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Add Items To Store"))
struct ARCMASSGAMEPLAYFRAMEWORK_API FArcMassPawnInitFragment_AddItems : public FArcMassPawnInitFragment
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = "Data", meta = (MetaStruct = "/Script/ArcMassItemsRuntime.ArcMassItemStoreFragment"))
	TObjectPtr<UScriptStruct> StoreType = nullptr;

	UPROPERTY(EditAnywhere, Category = "Data")
	TArray<FArcMassPawnAddItemEntry> Items;

public:
	virtual void Initialize(APawn* Pawn
							, APlayerState* PlayerState
							, APlayerController* PlayerController
							, FMassEntityManager* EntityManager
							, FMassEntityHandle Entity
							, UArcMassPawnExtensionComponent* OwnerExtension) const override;

	virtual ~FArcMassPawnInitFragment_AddItems() override = default;
};

USTRUCT(BlueprintType)
struct ARCMASSGAMEPLAYFRAMEWORK_API FArcMassPawnAddItemSlotEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Data", meta = (AllowedTypes = "ArcItem"))
	FArcNamedPrimaryAssetId Item;

	UPROPERTY(EditAnywhere, Category = "Data")
	uint8 Level = 1;

	UPROPERTY(EditAnywhere, Category = "Data")
	int32 Amount = 1;

	UPROPERTY(EditAnywhere, Category = "Data", meta = (Categories = "Slot"))
	FGameplayTag Slot;
};

/**
 * FArcMassPawnInitFragment_AddItemsToSlots
 *
 * Adds a list of items to a Mass item store and places each onto its specified slot
 * during DataInitialized. StoreType selects which store. Server-authority only.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Add Items To Slots"))
struct ARCMASSGAMEPLAYFRAMEWORK_API FArcMassPawnInitFragment_AddItemsToSlots : public FArcMassPawnInitFragment
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = "Data", meta = (MetaStruct = "/Script/ArcMassItemsRuntime.ArcMassItemStoreFragment"))
	TObjectPtr<UScriptStruct> StoreType = nullptr;

	UPROPERTY(EditAnywhere, Category = "Data")
	TArray<FArcMassPawnAddItemSlotEntry> Items;

public:
	virtual void Initialize(APawn* Pawn
							, APlayerState* PlayerState
							, APlayerController* PlayerController
							, FMassEntityManager* EntityManager
							, FMassEntityHandle Entity
							, UArcMassPawnExtensionComponent* OwnerExtension) const override;

	virtual ~FArcMassPawnInitFragment_AddItemsToSlots() override = default;
};

/**
 * FArcMassPawnInitFragment_AddItemsToBag
 *
 * Adds items to the entity's bag overlay: each item is added to the bag's TargetStoreType store,
 * then placed in the first free accepting bag slot. Items with no free slot stay in the store.
 * Server-authority only.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Add Items To Bag"))
struct ARCMASSGAMEPLAYFRAMEWORK_API FArcMassPawnInitFragment_AddItemsToBag : public FArcMassPawnInitFragment
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = "Data")
	TArray<FArcMassPawnAddItemEntry> Items;

public:
	virtual void Initialize(APawn* Pawn
							, APlayerState* PlayerState
							, APlayerController* PlayerController
							, FMassEntityManager* EntityManager
							, FMassEntityHandle Entity
							, UArcMassPawnExtensionComponent* OwnerExtension) const override;

	virtual ~FArcMassPawnInitFragment_AddItemsToBag() override = default;
};
