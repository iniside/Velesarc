/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2026 Lukasz Baran
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

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "UObject/PrimaryAssetId.h"
#include "GameplayTagContainer.h"

#include "ArcPersistenceTestTypes.generated.h"

/**
 * Simple flat struct for testing reflection serialization.
 * Properties marked SaveGame should be serialized; the unmarked one should not.
 */
USTRUCT()
struct FArcPersistenceTestStruct
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	int32 Health = 100;

	UPROPERTY(SaveGame)
	FString Name;

	UPROPERTY(SaveGame)
	float Speed = 0.0f;

	UPROPERTY(SaveGame)
	FGuid Id;

	UPROPERTY(SaveGame)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(SaveGame)
	TArray<int32> Scores;

	/** This property should NOT be serialized (no SaveGame flag). */
	UPROPERTY()
	int32 NotSaved = 42;
};

/**
 * Struct with a nested USTRUCT member for testing recursive reflection serialization.
 */
USTRUCT()
struct FArcPersistenceTestNestedStruct
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FArcPersistenceTestStruct Inner;

	UPROPERTY(SaveGame)
	float Multiplier = 1.0f;
};

/**
 * Struct with an array of USTRUCTs for testing array-of-struct serialization.
 */
USTRUCT()
struct FArcPersistenceTestArrayStruct
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	TArray<FArcPersistenceTestStruct> Items;

	UPROPERTY(SaveGame)
	int32 Count = 0;
};

// =============================================================================
// Mass Fragment test types for entity-level persistence integration tests.
// =============================================================================

/** Fragment with SaveGame + non-SaveGame properties. */
USTRUCT()
struct FArcTestHealthFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	int32 Health = 100;

	UPROPERTY(SaveGame)
	float Armor = 0.f;

	/** Should NOT be serialized (no SaveGame flag). */
	UPROPERTY()
	int32 TickCount = 0;
};

/** Fragment with container SaveGame properties. */
USTRUCT()
struct FArcTestInventoryFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	TArray<FName> Items;

	UPROPERTY(SaveGame)
	int32 Gold = 0;
};

template<>
struct TMassFragmentTraits<FArcTestInventoryFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

/** Fragment with zero SaveGame properties — should be auto-skipped. */
USTRUCT()
struct FArcTestTransientFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY()
	float Timer = 0.f;
};

// =============================================================================
// Complex property type test structs.
// =============================================================================

/** Test struct with all complex reference types. */
USTRUCT()
struct FArcPersistenceTestComplexStruct
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	TSoftObjectPtr<UObject> SoftRef;

	UPROPERTY(SaveGame)
	TSoftClassPtr<UObject> SoftClass;

	UPROPERTY(SaveGame)
	TSubclassOf<UObject> SubclassRef;

	UPROPERTY(SaveGame)
	TObjectPtr<UObject> HardRef = nullptr;

	UPROPERTY(SaveGame)
	FPrimaryAssetId AssetId;
};

/** Test struct with TMap properties. */
USTRUCT()
struct FArcPersistenceTestMapStruct
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	TMap<FString, int32> StringIntMap;

	UPROPERTY(SaveGame)
	TMap<FName, FString> NameStringMap;

	UPROPERTY(SaveGame)
	TMap<int32, FArcPersistenceTestStruct> IntStructMap;
};

/** Test struct with TSet properties. */
USTRUCT()
struct FArcPersistenceTestSetStruct
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	TSet<FName> NameSet;

	UPROPERTY(SaveGame)
	TSet<int32> IntSet;

	UPROPERTY(SaveGame)
	TSet<FString> StringSet;
};

/** Test struct with gameplay tag properties. */
USTRUCT()
struct FArcPersistenceTestTagStruct
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FGameplayTag SingleTag;

	UPROPERTY(SaveGame)
	FGameplayTagContainer TagContainer;

	UPROPERTY(SaveGame)
	FString Label;
};

/** Mass fragment with complex types for entity-level testing. */
USTRUCT()
struct FArcTestComplexFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	TSoftObjectPtr<UObject> AssetRef;

	UPROPERTY(SaveGame)
	TSubclassOf<AActor> ActorClass;

	UPROPERTY(SaveGame)
	FPrimaryAssetId PrimaryAsset;

	UPROPERTY(SaveGame)
	TMap<FName, int32> Stats;

	UPROPERTY(SaveGame)
	TSet<FName> Tags;
};

template<>
struct TMassFragmentTraits<FArcTestComplexFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
