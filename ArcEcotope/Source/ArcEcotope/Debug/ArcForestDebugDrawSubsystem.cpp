// Copyright Lukasz Baran. All Rights Reserved.

#include "Debug/ArcForestDebugDrawSubsystem.h"

#include "Climate/ArcClimateVolume.h"
#include "Debug/ArcForestDebugDraw.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Instances/ArcEcotopeInstancePartitionActor.h"
#include "Misc/App.h"
#include "Simulation/ArcEcotopeVolume.h"

bool UArcForestDebugDrawSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Super::ShouldCreateSubsystem(Outer) && FApp::CanEverRender();
}

bool UArcForestDebugDrawSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor;
}

void UArcForestDebugDrawSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LevelAddedHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(this, &UArcForestDebugDrawSubsystem::HandleLevelChanged);
	LevelRemovedHandle = FWorldDelegates::LevelRemovedFromWorld.AddUObject(this, &UArcForestDebugDrawSubsystem::HandleLevelChanged);

#if WITH_EDITOR
	if (GEngine != nullptr)
	{
		ActorAddedHandle = GEngine->OnLevelActorAdded().AddUObject(this, &UArcForestDebugDrawSubsystem::HandleLevelActorChanged);
		ActorDeletedHandle = GEngine->OnLevelActorDeleted().AddUObject(this, &UArcForestDebugDrawSubsystem::HandleLevelActorChanged);
		ActorListChangedHandle = GEngine->OnLevelActorListChanged().AddUObject(this, &UArcForestDebugDrawSubsystem::HandleLevelActorListChanged);
	}
#endif
}

void UArcForestDebugDrawSubsystem::Deinitialize()
{
	FWorldDelegates::LevelAddedToWorld.Remove(LevelAddedHandle);
	FWorldDelegates::LevelRemovedFromWorld.Remove(LevelRemovedHandle);
	LevelAddedHandle.Reset();
	LevelRemovedHandle.Reset();

#if WITH_EDITOR
	if (GEngine != nullptr)
	{
		GEngine->OnLevelActorAdded().Remove(ActorAddedHandle);
		GEngine->OnLevelActorDeleted().Remove(ActorDeletedHandle);
		GEngine->OnLevelActorListChanged().Remove(ActorListChangedHandle);
	}
	ActorAddedHandle.Reset();
	ActorDeletedHandle.Reset();
	ActorListChangedHandle.Reset();
#endif

	CachedVolumes.Reset();
	CachedClimateVolumes.Reset();
	CachedInstancePartitionActors.Reset();
	SiteReaderCache.Reset();
	GeometryCache.Reset();

	Super::Deinitialize();
}

TStatId UArcForestDebugDrawSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArcForestDebugDrawSubsystem, STATGROUP_Tickables);
}

void UArcForestDebugDrawSubsystem::HandleLevelChanged(ULevel* /*Level*/, UWorld* InWorld)
{
	if (InWorld == GetWorld())
	{
		MarkActorCacheDirty();
	}
}

#if WITH_EDITOR
void UArcForestDebugDrawSubsystem::HandleLevelActorChanged(AActor* Actor)
{
	if (Actor != nullptr && Actor->GetWorld() == GetWorld())
	{
		MarkActorCacheDirty();
	}
}

void UArcForestDebugDrawSubsystem::HandleLevelActorListChanged()
{
	MarkActorCacheDirty();
}
#endif

void UArcForestDebugDrawSubsystem::MarkActorCacheDirty()
{
	bActorCacheDirty = true;
}

void UArcForestDebugDrawSubsystem::RefreshActorCache(UWorld& World)
{
	CachedVolumes.Reset();
	for (TActorIterator<AArcEcotopeVolume> VolumeIt(&World); VolumeIt; ++VolumeIt)
	{
		if (IsValid(*VolumeIt))
		{
			CachedVolumes.Emplace(*VolumeIt);
		}
	}

	CachedClimateVolumes.Reset();
	for (TActorIterator<AArcClimateVolume> VolumeIt(&World); VolumeIt; ++VolumeIt)
	{
		if (IsValid(*VolumeIt))
		{
			CachedClimateVolumes.Emplace(*VolumeIt);
		}
	}

	CachedInstancePartitionActors.Reset();
	for (TActorIterator<AArcEcotopeInstancePartitionActor> PartitionIt(&World); PartitionIt; ++PartitionIt)
	{
		if (IsValid(*PartitionIt))
		{
			CachedInstancePartitionActors.Emplace(*PartitionIt);
		}
	}

	bActorCacheDirty = false;
}

void UArcForestDebugDrawSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	if (bActorCacheDirty)
	{
		RefreshActorCache(*World);
	}

	bool bAnyEnabled = false;
	for (const TWeakObjectPtr<AArcEcotopeVolume>& VolumePtr : CachedVolumes)
	{
		const AArcEcotopeVolume* Volume = VolumePtr.Get();
		if (Volume != nullptr && Volume->HasDebugOverlayEnabled())
		{
			bAnyEnabled = true;
			break;
		}
	}
	if (!bAnyEnabled)
	{
		for (const TWeakObjectPtr<AArcClimateVolume>& VolumePtr : CachedClimateVolumes)
		{
			const AArcClimateVolume* Volume = VolumePtr.Get();
			if (Volume != nullptr && Volume->HasDebugOverlayEnabled())
			{
				bAnyEnabled = true;
				break;
			}
		}
	}

	if (!bAnyEnabled)
	{
		return;
	}

	for (const TWeakObjectPtr<AArcEcotopeVolume>& VolumePtr : CachedVolumes)
	{
		const AArcEcotopeVolume* Volume = VolumePtr.Get();
		if (Volume != nullptr && Volume->HasDebugOverlayEnabled())
		{
			ArcForestDebugDraw::DrawVolume(
				*Volume, SiteReaderCache, GeometryCache, CachedInstancePartitionActors);
		}
	}

	for (const TWeakObjectPtr<AArcClimateVolume>& VolumePtr : CachedClimateVolumes)
	{
		const AArcClimateVolume* Volume = VolumePtr.Get();
		if (Volume != nullptr && Volume->HasDebugOverlayEnabled())
		{
			ArcForestDebugDraw::DrawVolume(*Volume, GeometryCache);
		}
	}
}
