/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
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

#include "GameFramework/Actor.h"
#include "ArcCraft/Station/ArcCraftStationComponent.h"
#include "Items/ArcItemsStoreComponent.h"
#include "Items/ArcItemDefinition.h"

#include "ArcCraftStationIntegrationTests.generated.h"

// ===================================================================
// Subclassed stores to allow TSubclassOf matching in tests
// ===================================================================

UCLASS()
class UArcCraftStationTestInputStore : public UArcItemsStoreComponent
{
	GENERATED_BODY()
};

UCLASS()
class UArcCraftStationTestOutputStore : public UArcItemsStoreComponent
{
	GENERATED_BODY()
};

UCLASS()
class UArcCraftStationTestInstigatorStore : public UArcItemsStoreComponent
{
	GENERATED_BODY()
};

// ===================================================================
// Testable station component — exposes protected members for tests
// and resolves transient item definitions for output specs.
// ===================================================================

UCLASS()
class UArcCraftStationTestComponent : public UArcCraftStationComponent
{
	GENERATED_BODY()
public:
	/**
	 * Registry mapping FPrimaryAssetId to transient UArcItemDefinition objects.
	 * Used by BuildOutputSpec to set ItemDefinitionAsset on output specs,
	 * enabling the full AddItem pipeline to work without the asset manager.
	 */
	TMap<FPrimaryAssetId, TObjectPtr<const UArcItemDefinition>> TransientDefinitions;

	void RegisterTransientDefinition(const UArcItemDefinition* Def)
	{
		if (Def)
		{
			TransientDefinitions.Add(Def->GetPrimaryAssetId(), Def);
		}
	}

	void SetStationTags(const FGameplayTagContainer& InTags)
	{
		StationTags = InTags;
	}

	void SetTimeMode(EArcCraftStationTimeMode InMode)
	{
		TimeMode = InMode;
	}

	void SetMaxQueueSize(int32 InSize)
	{
		MaxQueueSize = InSize;
	}

	void SetTickInterval(float InInterval)
	{
		TickInterval = InInterval;
	}

	void SetItemSource(const TInstancedStruct<FArcCraftItemSource>& InSource)
	{
		ItemSource = InSource;
	}

	void SetOutputDelivery(const TInstancedStruct<FArcCraftOutputDelivery>& InDelivery)
	{
		OutputDelivery = InDelivery;
	}

	/** Simulate tick for testing without requiring a full world tick. */
	void SimulateTick(float DeltaTime)
	{
		TickComponent(DeltaTime, LEVELTICK_All, nullptr);
	}

protected:
	/**
	 * Override to inject transient item definitions onto the output spec.
	 * This ensures the full AddItem pipeline works in tests without the asset manager.
	 */
	virtual FArcItemSpec BuildOutputSpec(
		const UArcRecipeDefinition* Recipe,
		const UObject* Instigator) const override
	{
		FArcItemSpec OutputSpec = Super::BuildOutputSpec(Recipe, Instigator);

		// If the output spec doesn't already have an ItemDefinition resolved,
		// try to find it in the transient definitions registry.
		if (OutputSpec.ItemDefinition == nullptr)
		{
			const FPrimaryAssetId OutputId = OutputSpec.GetItemDefinitionId();
			if (const TObjectPtr<const UArcItemDefinition>* Found = TransientDefinitions.Find(OutputId))
			{
				OutputSpec.SetItemDefinitionAsset(*Found);
			}
		}

		return OutputSpec;
	}
};

// ===================================================================
// Test actors
// ===================================================================

UCLASS()
class AArcCraftStationTestableActor : public AActor
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TObjectPtr<UArcCraftStationTestComponent> StationComponent;

	UPROPERTY()
	TObjectPtr<UArcCraftStationTestInputStore> InputStore;

	UPROPERTY()
	TObjectPtr<UArcCraftStationTestOutputStore> OutputStore;

	AArcCraftStationTestableActor()
	{
		StationComponent = CreateDefaultSubobject<UArcCraftStationTestComponent>(TEXT("StationComponent"));
		InputStore = CreateDefaultSubobject<UArcCraftStationTestInputStore>(TEXT("InputStore"));
		OutputStore = CreateDefaultSubobject<UArcCraftStationTestOutputStore>(TEXT("OutputStore"));
	}
};

UCLASS()
class AArcCraftStationTestInstigator : public AActor
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TObjectPtr<UArcCraftStationTestInstigatorStore> InventoryStore;

	AArcCraftStationTestInstigator()
	{
		InventoryStore = CreateDefaultSubobject<UArcCraftStationTestInstigatorStore>(TEXT("InventoryStore"));
	}
};
