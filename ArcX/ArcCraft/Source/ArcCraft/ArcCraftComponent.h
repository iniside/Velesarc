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
#include "StructUtils/InstancedStruct.h"

#include "ArcCraftComponent.generated.h"

class UArcCraftComponent;
class UArcItemsStoreComponent;

USTRUCT()
struct ARCCRAFT_API FArcCraftRequirement
{
	GENERATED_BODY()

public:
	virtual bool MeetsRequirement(UArcCraftComponent* InCraftComponent, const UArcCraftData* InCraftData, UObject* InOwner) const
	{
		return true; // Default implementation, can be overridden
	}
	
	virtual ~FArcCraftRequirement() = default;
};

USTRUCT()
struct FArcCraftItemDefinitionCount
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcNamedPrimaryAssetId ItemDefinition;

	UPROPERTY()
	uint16 Count = 1;
};

USTRUCT()
struct ARCCRAFT_API FArcCraftRequirement_ItemDefinitionCount : public FArcCraftRequirement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TArray<FArcCraftItemDefinitionCount> ItemDefinitions;
	
public:
	virtual bool MeetsRequirement(UArcCraftComponent* InCraftComponent, const UArcCraftData* InCraftData, const UObject* InOwner) const override;

	virtual ~FArcCraftRequirement_ItemDefinitionCount() override = default;
};

struct FArcItemSpec;

USTRUCT()
struct ARCCRAFT_API FArcCraftExecution
{
	GENERATED_BODY()

public:
	virtual void OnCraftFinished(UArcCraftComponent* InCraftComponent, const UArcCraftData* InCraftData, const UObject* InOwner, FArcItemSpec& ItemSpec) const
	{
	}
	
	virtual ~FArcCraftExecution() = default;
};

USTRUCT()
struct ARCCRAFT_API FArcCraftExecution_MakeItemSpec : public FArcCraftExecution
{
	GENERATED_BODY()

public:
	virtual void OnCraftFinished(UArcCraftComponent* InCraftComponent, const UArcCraftData* InCraftData, const UObject* InOwner, FArcItemSpec& ItemSpec) const override;

	virtual ~FArcCraftExecution_MakeItemSpec() override = default;
};

USTRUCT()
struct ARCCRAFT_API FArcCraftExecution_AddToCraftingStore : public FArcCraftExecution
{
	GENERATED_BODY()

public:
	virtual void OnCraftFinished(UArcCraftComponent* InCraftComponent, const UArcCraftData* InCraftData, const UObject* InOwner, FArcItemSpec& ItemSpec) const override;

	virtual ~FArcCraftExecution_AddToCraftingStore() override = default;
};

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

	// What is required to craft this item.
	UPROPERTY(EditAnywhere)
	TArray<TInstancedStruct<FArcCraftRequirement>> Requirements;

	// What will happen when crafting finished.
	UPROPERTY(EditAnywhere)
	TArray<TInstancedStruct<FArcCraftExecution>> OnFinishedExecutions;
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
	TObjectPtr<const UObject> Instigator;
	
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

USTRUCT()
struct ARCCRAFT_API FArcCraftedItemSpecList : public FIrisFastArraySerializer
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
	
	typedef UE::Net::TIrisFastArrayEditor<FArcCraftedItemSpecList> FFastArrayEditor;
	
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

	UPROPERTY(Replicated)
	FArcCraftedItemSpecList CraftedItemSpecList;;
	
	UPROPERTY(EditAnywhere)
	int32 MaxQueuedCraftedItems;
	
	UPROPERTY(EditAnywhere)
	int32 MaxCraftedItemsAtOneTime;
	
	TArray<FArcCraftItem> PendingAdds;

public:
	// Sets default values for this component's properties
	UArcCraftComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	void UpdateStatus();
	
public:
	void CraftItem(const UArcCraftData* InCraftData, UObject* Instigator, int32 Amount = 1, int32 Priority = 0);

	void OnCraftFinished(const FArcCraftItem& CraftedItem);
	

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	const TArray<FArcCraftItem>& GetCraftedItemList() const
	{
		return CraftedItemList.GetItemArray();
	}

	FArcCraftedItemSpecList& GetCraftedItemSpecList()
	{
		return CraftedItemSpecList;
	}
};
