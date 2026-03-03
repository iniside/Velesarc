// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcEntitySpawnerTypes.h"
#include "Serialization/ArcPersistenceSerializer.h"

class FArcSaveArchive;
class FArcLoadArchive;
class UWorld;

/**
 * Serializer for FArcSpawnedByFragment.
 * Saves only SpawnerGuid. SpawnerEntity handle is runtime-only.
 */
struct FArcSpawnedByFragmentSerializer
{
	using SourceType = FArcSpawnedByFragment;
	static constexpr uint32 Version = 1;

	static void Save(const FArcSpawnedByFragment& Src, FArcSaveArchive& Ar);
	static void Load(FArcSpawnedByFragment& Dst, FArcLoadArchive& Ar);
};

UE_ARC_DECLARE_PERSISTENCE_SERIALIZER(FArcSpawnedByFragmentSerializer, ARCENTITYSPAWNER_API);

/**
 * Serializer for FArcSpawnerFragment.
 * Saves only SpawnedEntityGuids. SpawnedEntities handles are runtime-only.
 */
struct FArcSpawnerFragmentSerializer
{
	using SourceType = FArcSpawnerFragment;
	static constexpr uint32 Version = 1;

	static void Save(const FArcSpawnerFragment& Src, FArcSaveArchive& Ar);
	static void Load(FArcSpawnerFragment& Dst, FArcLoadArchive& Ar);
	static void PostLoad(FArcSpawnerFragment& Dst, UWorld& World);
};

UE_ARC_DECLARE_PERSISTENCE_SERIALIZER(FArcSpawnerFragmentSerializer, ARCENTITYSPAWNER_API);
