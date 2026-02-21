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

#include "ArcNamedPrimaryAssetId.h"
#include "Engine/DataAsset.h"
#include "StructUtils/InstancedStruct.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment.h"
#include "ArcItemSpecGeneratorDefinition.generated.h"

class UArcItemSpecGeneratorDefinition;
class UArcItemGeneratorDefinition;
class AActor;
class APlayerController;
struct FArcItemSpecGeneratorRowEntry;

// ---------------------------------------------------------------------------
// Item Generator (moved from old ArcLootSubsystem.h)
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Item Spec Generator
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCCORE_API FArcItemSpecGenerator
{
	GENERATED_BODY()

public:
	virtual FArcItemSpec GenerateItemSpec(UArcItemSpecGeneratorDefinition* FactoryData, const FArcNamedPrimaryAssetId& Row, AActor* From, APlayerController* For) const;

	virtual ~FArcItemSpecGenerator() = default;
};

USTRUCT()
struct ARCCORE_API FArcItemGenerator_SpecDefinitionSingleItem : public FArcItemGenerator
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Data", meta = (DisplayThumbnail = false))
	TObjectPtr<UArcItemSpecGeneratorDefinition> SpecDefinition = nullptr;

	UPROPERTY(EditAnywhere, Category = "Data", meta = (BaseStruct = "/Script/ArcCore.ArcItemSpecGenerator", ExcludeBaseStruct))
	FInstancedStruct ItemGenerator;

public:
	virtual void GenerateItems(TArray<FArcItemSpec>& OutItems, const UArcItemGeneratorDefinition* InDef, AActor* From, class APlayerController* For) const override;

	virtual ~FArcItemGenerator_SpecDefinitionSingleItem() override = default;
};


/**
 *
 */
UCLASS()
class ARCCORE_API UArcItemSpecGeneratorDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Data", meta = (DisplayThumbnail = false))
	TObjectPtr<class UArcItemFactoryGrantedEffects> GrantedEffects;

	UPROPERTY(EditAnywhere, Category = "Data", meta = (DisplayThumbnail = false))
	TObjectPtr<class UArcItemFactoryGrantedPassiveAbilities> GrantedPassiveAbilities;

	UPROPERTY(EditAnywhere, Category = "Data", meta = (DisplayThumbnail = false))
	TObjectPtr<class UArcItemFactoryStats> ItemStats;

	UPROPERTY(EditAnywhere, Category = "Data", meta = (DisplayThumbnail = false))
	TObjectPtr<class UArcItemFactoryAttributes> Attributes;
};

USTRUCT()
struct FArcItemFragment_ItemGenerator : public FArcItemFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Data")
	TObjectPtr<UArcItemSpecGeneratorDefinition> ItemGeneratorDef;
};

USTRUCT()
struct ARCCORE_API FArcItemSpecGenerator_RandomItemStats : public FArcItemSpecGenerator
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	int32 MaxStats = 1;

public:
	virtual FArcItemSpec GenerateItemSpec(UArcItemSpecGeneratorDefinition* FactoryData
		, const FArcNamedPrimaryAssetId& Row
		, AActor* From
		, APlayerController* For) const override;
};
