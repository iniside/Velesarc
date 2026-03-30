// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityHandle.h"
#include "MassEntityTypes.h"

class FArcSaveArchive;
class FArcLoadArchive;
struct FMassEntityManager;
struct FArcMassPersistenceConfigFragment;
class UWorld;

/**
 * Serializes/deserializes a set of Mass entity fragments to/from
 * ArcPersistence archives. Each fragment is stored as a named struct
 * in the archive, keyed by the UScriptStruct name.
 *
 * Fragment selection respects the allow/disallow config.
 * Falls back to reflection serialization (SaveGame properties).
 */
class ARCMASS_API FArcMassFragmentSerializer
{
public:
	/**
	 * Serialize an entity's configured fragments into the archive.
	 * Writes a "fragments" array into the current archive scope.
	 */
	static void SaveEntityFragments(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const FArcMassPersistenceConfigFragment& Config,
		FArcSaveArchive& Ar,
		UWorld* World = nullptr);

	/**
	 * Deserialize fragments from the archive and apply to an entity.
	 * Reads a "fragments" array from the current archive scope.
	 */
	static void LoadEntityFragments(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FArcLoadArchive& Ar,
		UWorld* World = nullptr);

	/** Serialize a single UScriptStruct fragment. */
	static void SaveFragment(
		const UScriptStruct* FragmentType,
		const void* Data,
		FArcSaveArchive& Ar,
		bool bForceAllProperties = false);

	/** Deserialize a single UScriptStruct fragment. */
	static void LoadFragment(
		const UScriptStruct* FragmentType,
		void* Data,
		FArcLoadArchive& Ar);

	/** Check if a fragment type should be serialized given the config. */
	static bool ShouldSerializeFragment(
		const UScriptStruct* FragmentType,
		const FArcMassPersistenceConfigFragment& Config);
};
