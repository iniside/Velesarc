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

#include "Items/ArcItemDefinition.h"
#include "ArcCore/Items/ArcItemData.h"

#include "Engine/DataAsset.h"
#include "Items/ArcItemSpec.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcLootSubsystem.generated.h"

class UArcItemSpecGeneratorDefinition;
class AArcSpawnedItemActor;
class APlayerController;
class UArcItemListTable;

USTRUCT()
struct FArcSelectedItemSpecContainer
{
	GENERATED_BODY()

	TArray<FArcItemSpec> SelectedItems;
};

USTRUCT()
struct ARCCORE_API FArcItemRowEntry
{
	GENERATED_BODY()

public:
	virtual FArcItemSpec GetItem(AActor* From, class APlayerController* For) const
	{
		return FArcItemSpec();
	}

	virtual ~FArcItemRowEntry() = default;
};

USTRUCT()
struct ARCCORE_API FArcItemDataRowEntry : public FArcItemRowEntry
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	mutable FArcNamedPrimaryAssetId ItemDefinitionId;

	virtual FArcItemSpec GetItem(AActor* From, APlayerController* For) const override;

	virtual ~FArcItemDataRowEntry() override = default;
};

USTRUCT()
struct ARCCORE_API FArcItemSpecGeneratorRowEntry : public FArcItemRowEntry
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Data", meta = (DisplayThumbnail = false))
	TObjectPtr<UArcItemSpecGeneratorDefinition> Entry = nullptr;

	UPROPERTY(EditAnywhere, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId ItemDefinitionId;
	
	UPROPERTY(EditAnywhere, Category = "Data", meta = (BaseStruct = "/Script/ArcCore.ArcItemSpecGenerator"))
	FInstancedStruct ItemGenerator;
	
	virtual FArcItemSpec GetItem(AActor* From, APlayerController* For) const override;
	
	virtual ~FArcItemSpecGeneratorRowEntry() override = default;
};

/*
 * Override and implement function in this struct to Get if list Item Specs.
 * The actual items in list are generated using ArcItemRowEntry, this class should just
 * pick items from SpawnTable.
 */
USTRUCT()
struct ARCCORE_API FArcGetItemsFromTable
{
	GENERATED_BODY()

public:
	virtual FArcSelectedItemSpecContainer GenerateItems(UArcItemListTable* InLootTable
														, AActor* From
														, APlayerController* For) const
	{
		return FArcSelectedItemSpecContainer();
	};

	virtual ~FArcGetItemsFromTable()
	{
	}
};

USTRUCT()
struct ARCCORE_API FArcGetItemsFromTable_Random : public FArcGetItemsFromTable
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere)
	int32 MaxItems = 2;

public:
	virtual FArcSelectedItemSpecContainer GenerateItems(class UArcItemListTable* InLootTable
														, AActor* From
														, APlayerController* For) const override;

	virtual ~FArcGetItemsFromTable_Random() override
	{
	}
};

USTRUCT()
struct ARCCORE_API FArcItemGenerator
{
	GENERATED_BODY()

public:
	virtual void GenerateItems(TArray<FArcItemSpec>& OutItems, const UArcItemGeneratorDefinition* InDef, AActor* From, class APlayerController* For) const
	{
	}

	virtual ~FArcItemGenerator() = default;
};

USTRUCT()
struct ARCCORE_API FArcItemGenerator_SingleItem : public FArcItemGenerator
{
	GENERATED_BODY()

public:
	virtual void GenerateItems(TArray<FArcItemSpec>& OutItems, const UArcItemGeneratorDefinition* InDef, AActor* From, class APlayerController* For) const override;

	virtual ~FArcItemGenerator_SingleItem() override = default;
};

/**
 * @brief Contains list of specific item generators as InstancedStruct.
 * Each Row can implement it's own way of proving FArcItemSpec, which is final spec of item, used to create instance.
 */
UCLASS()
class ARCCORE_API UArcItemGeneratorDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * List of item definitions that can be used to generate item.
	 */
	UPROPERTY(EditAnywhere, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	TArray<FArcNamedPrimaryAssetId> ItemDefinitions;

	/**
	 * Item generator which can generator item specs based on list of Item Definitions.
	 */
	UPROPERTY(EditAnywhere, Category = "Data", meta = (BaseStruct = "/Script/ArcCore.ArcItemGenerator", ExcludeBaseStruct))
	FInstancedStruct ItemGenerator;

	void GetItems(TArray<FArcItemSpec>& OutItems, AActor* From, APlayerController* For) const;
};

USTRUCT()
struct ARCCORE_API FArcItemSpawnDataRow
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	float Weight = 1;

	//UPROPERTY(EditAnywhere, Category = "Data", meta = (BaseStruct = "/Script/ArcCore.ArcItemRowEntry", ExcludeBaseStruct))
	//FInstancedStruct ItemRow;

	UPROPERTY(EditAnywhere, Category = "Data", meta = (DisplayThumbnail = false))
	TObjectPtr<UArcItemGeneratorDefinition> ItemGenerator;
};

/**
 * Select Item first
 * Or select rarity first ?
 */
UCLASS()
class ARCCORE_API UArcItemListTable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// select type of item
	// weapon, armor, what family of armor etc.
	// Different Loot Datas need different weights.
	UPROPERTY(EditAnywhere, Category = "Data")
	TArray<FArcItemSpawnDataRow> ItemSpawnDataRows;

	/*
	 * Provide how items from this SpawnTable will be selected.
	 */
	UPROPERTY(EditAnywhere, Category = "Data", meta = (BaseStruct = "/Script/ArcCore.ArcGetItemsFromTable", ExcludeBaseStruct))
	FInstancedStruct ItemGetter;

};

/**
 *
 */
UCLASS()
class ARCCORE_API UArcLootSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	FArcSelectedItemSpecContainer GenerateItemsFor(UArcItemListTable* LootTable
				   		  , AActor* From
				   		  , APlayerController* For);
};
