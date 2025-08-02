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
#include "Components/ActorComponent.h"
#include "Iris/ReplicationState/IrisFastArraySerializer.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Iris/ReplicationState/Private/IrisFastArraySerializerInternal.h"

#include "ArcCraftComponent.generated.h"

class UArcItemsStoreComponent;
/**
 * You can call it recipe. It is information needed to create Item.
 */
UCLASS(BlueprintType)
class ARCCRAFT_API UArcCraftData : public UDataAsset
{
	GENERATED_BODY()

public:
	// How much time it will take to craft single item in seconds
	UPROPERTY(EditAnywhere)
	int32 CraftTime;

	// Item Definition used as base for crafted item.
	UPROPERTY(EditAnywhere, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition"))
	FArcNamedPrimaryAssetId ItemDefinition;
};

USTRUCT()
struct ARCCRAFT_API FArcCraftItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid Id;
	
	UPROPERTY()
	TObjectPtr<const UArcCraftData> Recipe;

	UPROPERTY()
	int32 MaxAmount = 0;

	UPROPERTY()
	int32 CurrentAmount = 0;

	UPROPERTY()
	int64 StartTime = 0;

	UPROPERTY()
	int64 UnpausedEndTime = 0;

	UPROPERTY()
	float CurrentTime = 0;

	// The lower the number the higher priority
	UPROPERTY()
	int32 Priority = 0;
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

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ARCCRAFT_API UArcCraftComponent : public UActorComponent
{
	GENERATED_BODY()

protected:
	UPROPERTY(Replicated)
	FArcCraftItemList CraftedItemList;

	UPROPERTY(EditAnywhere)
	int32 MaxQueuedCraftedItems;
	
	UPROPERTY(EditAnywhere)
	int32 MaxCraftedItemsAtOneTime;
	
	TArray<FArcCraftItem> PendingAdds;

	mutable TWeakObjectPtr<UArcItemsStoreComponent> ItemsStoreComponent;
public:
	// Sets default values for this component's properties
	UArcCraftComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	void UpdateStatus();
	
public:
	void CraftItem(const UArcCraftData* InCraftData, int32 Amount = 1, int32 Priority = 0);

	void OnCraftFinished(const FArcCraftItem& CraftedItem);
	

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	const TArray<FArcCraftItem>& GetCraftedItemList() const
	{
		return CraftedItemList.GetItemArray();
	}

	UArcItemsStoreComponent* GetItemsStore() const;
};
