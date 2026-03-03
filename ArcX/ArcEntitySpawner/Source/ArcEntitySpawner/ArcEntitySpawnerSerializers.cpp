// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEntitySpawnerSerializers.h"

#include "ArcEntitySpawnerSubsystem.h"
#include "ArcMass/Persistence/ArcMassEntityPersistenceSubsystem.h"
#include "MassEntitySubsystem.h"
#include "Engine/World.h"
#include "Serialization/ArcSaveArchive.h"
#include "Serialization/ArcLoadArchive.h"

// ── FArcSpawnedByFragmentSerializer ─────────────────────────────────────

void FArcSpawnedByFragmentSerializer::Save(const FArcSpawnedByFragment& Src, FArcSaveArchive& Ar)
{
	Ar.WriteProperty(FName("SpawnerGuid"), Src.SpawnerGuid);
}

void FArcSpawnedByFragmentSerializer::Load(FArcSpawnedByFragment& Dst, FArcLoadArchive& Ar)
{
	Ar.ReadProperty(FName("SpawnerGuid"), Dst.SpawnerGuid);
}

UE_ARC_IMPLEMENT_PERSISTENCE_SERIALIZER(FArcSpawnedByFragmentSerializer, "FArcSpawnedByFragment");

// ── FArcSpawnerFragmentSerializer ───────────────────────────────────────

void FArcSpawnerFragmentSerializer::Save(const FArcSpawnerFragment& Src, FArcSaveArchive& Ar)
{
	Ar.BeginArray(FName("guids"), Src.SpawnedEntityGuids.Num());
	for (int32 i = 0; i < Src.SpawnedEntityGuids.Num(); ++i)
	{
		Ar.BeginArrayElement(i);
		Ar.WriteProperty(FName("g"), Src.SpawnedEntityGuids[i]);
		Ar.EndArrayElement();
	}
	Ar.EndArray();
}

void FArcSpawnerFragmentSerializer::Load(FArcSpawnerFragment& Dst, FArcLoadArchive& Ar)
{
	int32 Count = 0;
	if (Ar.BeginArray(FName("guids"), Count))
	{
		Dst.SpawnedEntityGuids.Reserve(Count);
		for (int32 i = 0; i < Count; ++i)
		{
			if (Ar.BeginArrayElement(i))
			{
				FGuid Guid;
				Ar.ReadProperty(FName("g"), Guid);
				Dst.SpawnedEntityGuids.Add(Guid);
				Ar.EndArrayElement();
			}
		}
		Ar.EndArray();
	}
}

void FArcSpawnerFragmentSerializer::PostLoad(FArcSpawnerFragment& Dst, UWorld& World)
{
	// Reconcile: look up persisted GUIDs in the persistence subsystem
	// and repopulate the runtime entity handles.
	UArcMassEntityPersistenceSubsystem* PersistSub =
		World.GetSubsystem<UArcMassEntityPersistenceSubsystem>();
	UMassEntitySubsystem* MassSub =
		World.GetSubsystem<UMassEntitySubsystem>();

	if (!PersistSub || !MassSub)
	{
		return;
	}

	FMassEntityManager& EntityManager = MassSub->GetMutableEntityManager();

	TArray<FGuid> StillAliveGuids;
	Dst.SpawnedEntities.Reset();

	for (const FGuid& EntityGuid : Dst.SpawnedEntityGuids)
	{
		if (const FMassEntityHandle* Handle = PersistSub->ActiveEntities.Find(EntityGuid))
		{
			if (EntityManager.IsEntityValid(*Handle))
			{
				Dst.SpawnedEntities.Add(*Handle);
				StillAliveGuids.Add(EntityGuid);
			}
		}
	}

	Dst.SpawnedEntityGuids = MoveTemp(StillAliveGuids);
}

UE_ARC_IMPLEMENT_PERSISTENCE_SERIALIZER(FArcSpawnerFragmentSerializer, "FArcSpawnerFragment");
