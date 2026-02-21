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
#include "Components/ActorComponent.h"
#include "Iris/ReplicationState/IrisFastArraySerializer.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Iris/ReplicationState/Private/IrisFastArraySerializerInternal.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcCraftComponent.generated.h"

class UArcCraftComponent;
class UArcItemsStoreComponent;
class UArcItemDefinition;
class UArcRecipeDefinition;

USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcCraftItemAmount
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FPrimaryAssetId ItemDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Amount = 1;
};

USTRUCT()
struct ARCCRAFT_API FArcCraftExecution
{
	GENERATED_BODY()

public:
	virtual bool CanCraft(const UArcCraftComponent* InCraftComponent, const UObject* InInstigator) const
	{
		return  true;
	}
	
	virtual bool CheckAndConsumeItems(const UArcCraftComponent* InCraftComponent, const UArcItemDefinition* InCraftData, const UObject* InInstigator, bool bConsume) const
	{
		return  true;
	}

	virtual TArray<FArcCraftItemAmount> AvailableItems(const UArcCraftComponent* InCraftComponent, const UArcItemDefinition* InCraftData, const UObject* InInstigator) const { return {}; }
	
	virtual FArcItemSpec OnCraftFinished(UArcCraftComponent* InCraftComponent, const UArcItemDefinition* InCraftData, const UObject* InInstigator) const
	{
		return FArcItemSpec();
	}
	
	virtual ~FArcCraftExecution() = default;
};



USTRUCT()
struct ARCCRAFT_API FArcCraftRequirement_InstigatorItemsStore : public FArcCraftExecution
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	TSubclassOf<UArcItemsStoreComponent> ItemsStoreClass;
	
	virtual bool CheckAndConsumeItems(const UArcCraftComponent* InCraftComponent, const UArcItemDefinition* InCraftData, const UObject* InInstigator, bool bConsume) const override;

	virtual TArray<FArcCraftItemAmount> AvailableItems(const UArcCraftComponent* InCraftComponent, const UArcItemDefinition* InCraftData, const UObject* InInstigator) const override;

	virtual FArcItemSpec OnCraftFinished(UArcCraftComponent* InCraftComponent, const UArcItemDefinition* InCraftData, const UObject* InInstigator) const override;
	
	virtual ~FArcCraftRequirement_InstigatorItemsStore() override = default;
};

USTRUCT(BlueprintType)
struct FArcItemFragment_CraftData : public FArcItemFragment
{
	GENERATED_BODY()

public:
	// How much time it will take to craft single item in seconds
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 CraftTime = 0;

	// Item Definition used as base for crafted item.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail= false))
	FArcNamedPrimaryAssetId ItemDefinition;
};

USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcCraftItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid Id;
	
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<const UArcItemDefinition> Recipe;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<const UObject> Instigator;
	
	UPROPERTY(BlueprintReadOnly)
	int32 MaxAmount = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 CurrentAmount = 0;

	UPROPERTY(BlueprintReadOnly)
	int64 StartTime = 0;

	UPROPERTY(BlueprintReadOnly)
	int64 UnpausedEndTime = 0;

	UPROPERTY(BlueprintReadOnly)
	float CurrentTime = 0;

	// The lower the number the higher priority
	UPROPERTY(BlueprintReadOnly)
	int32 Priority = 0;

	/** If set, this craft uses the recipe-based path instead of the legacy item-fragment path. */
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<const UArcRecipeDefinition> RecipeDefinition;
};

USTRUCT()
struct ARCCRAFT_API FArcCraftItemList : public FIrisFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FArcCraftItem> Items;

	using ItemArrayType = TArray<FArcCraftItem>;
	
	const ItemArrayType& GetItemArray() const
	{
		return Items;
	}
	
	ItemArrayType& GetItemArray()
	{
		return Items;
	}
	
	typedef UE::Net::TIrisFastArrayEditor<FArcCraftItemList> FFastArrayEditor;
	
	FFastArrayEditor Edit()
	{
		return FFastArrayEditor(*this);
	}
};

USTRUCT()
struct ARCCRAFT_API FArcCraftItemSpecList : public FIrisFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FArcItemSpec> Items;

	using ItemArrayType = TArray<FArcItemSpec>;
	
	const ItemArrayType& GetItemArray() const
	{
		return Items;
	}
	
	ItemArrayType& GetItemArray()
	{
		return Items;
	}
	
	typedef UE::Net::TIrisFastArrayEditor<FArcCraftItemSpecList> FFastArrayEditor;
	
	FFastArrayEditor Edit()
	{
		return FFastArrayEditor(*this);
	}
};

USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcCraftMessage
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite)
	TWeakObjectPtr<UArcCraftComponent> CraftComponent;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FArcCraftDynamicDelegate, UArcCraftComponent*, Component, const FArcCraftItem&, CraftedItem);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FArcCraftFinishedDynamicDelegate, UArcCraftComponent*, Component, const UArcItemDefinition*, Recipe, const FArcItemSpec&, ItemSpec);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ARCCRAFT_API UArcCraftComponent : public UActorComponent
{
	GENERATED_BODY()

protected:
	UPROPERTY(Replicated)
	FArcCraftItemList CraftedItemList;

	// TODO:: Maybe use ItemsStore for it ?
	UPROPERTY(Replicated)
	FArcCraftItemSpecList StoredResourceItemList;
	
	UPROPERTY(Replicated)
	FArcCraftItemSpecList CraftedItemSpecList;

	// This craft component, can only craft from recipies, which have these tags.
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer ItemTags;
	
	UPROPERTY(EditAnywhere)
	int32 MaxQueuedCraftedItems = 1;
	
	UPROPERTY(EditAnywhere)
	int32 MaxCraftedItemsAtOneTime = 5;
	
	TArray<FArcCraftItem> PendingAdds;

	UPROPERTY()
	TObjectPtr<AActor> PlayerOwner;

	TWeakObjectPtr<UObject> CurrentInstigator;
	
	int32 HighestPriorityIndex = INDEX_NONE;

	// What is required to craft this item. Executed In order.
	UPROPERTY(EditAnywhere)
	TInstancedStruct<FArcCraftExecution> CraftExecution;

	UPROPERTY(BlueprintAssignable)
	FArcCraftDynamicDelegate OnCraftItemAdded;
	
	UPROPERTY(BlueprintAssignable)
	FArcCraftDynamicDelegate OnCraftStarted;

	UPROPERTY(BlueprintAssignable)
	FArcCraftFinishedDynamicDelegate OnCraftFinishedDelegate;
	
public:
	// Sets default values for this component's properties
	UArcCraftComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	void UpdateStatus();

	virtual bool CheckRequirements(const UArcItemDefinition* InCraftData, const UObject* Instigator) const;

	bool CheckRecipeRequirements(const UArcRecipeDefinition* InRecipe, const UObject* Instigator) const;

	virtual void ConsumeRequiredItems(const UArcItemDefinition* InCraftData, const UObject* Instigator) {};

public:
	void CraftItem(const UArcItemDefinition* InCraftData, UObject* Instigator, int32 Amount = 1, int32 Priority = 0);

	/** Queue a recipe-based craft. */
	void CraftRecipe(const UArcRecipeDefinition* InRecipe, UObject* Instigator, int32 Amount = 1, int32 Priority = 0);

	void OnCraftFinished(const FArcCraftItem& CraftedItem);

	UFUNCTION(BlueprintCallable, Category = "Arc Craft")
	bool DoesHaveItemsToCraft(const UArcItemDefinition* InRecipe, UObject* InOwner) const;

	UFUNCTION(BlueprintCallable, Category = "Arc Craft")
	TArray<FArcCraftItemAmount> GetAvailableItems(const UArcItemDefinition* InRecipe, UObject* InOwner) const;
	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	const TArray<FArcCraftItem>& GetCraftedItemList() const
	{
		return CraftedItemList.GetItemArray();
	}

	FArcCraftItemSpecList& GetCraftedItemSpecList()
	{
		return CraftedItemSpecList;
	}
};
