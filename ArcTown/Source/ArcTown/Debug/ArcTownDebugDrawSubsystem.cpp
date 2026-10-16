// Copyright Lukasz Baran. All Rights Reserved.

#include "Debug/ArcTownDebugDrawSubsystem.h"

#include "Debug/ArcTownDebugDraw.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/App.h"
#include "Town/ArcTownAttractorVolume.h"
#include "Town/ArcTownVolume.h"

bool UArcTownDebugDrawSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Super::ShouldCreateSubsystem(Outer) && FApp::CanEverRender();
}

bool UArcTownDebugDrawSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor;
}

void UArcTownDebugDrawSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LevelAddedHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(this, &UArcTownDebugDrawSubsystem::HandleLevelChanged);
	LevelRemovedHandle = FWorldDelegates::LevelRemovedFromWorld.AddUObject(this, &UArcTownDebugDrawSubsystem::HandleLevelChanged);

#if WITH_EDITOR
	if (GEngine != nullptr)
	{
		ActorAddedHandle = GEngine->OnLevelActorAdded().AddUObject(this, &UArcTownDebugDrawSubsystem::HandleLevelActorChanged);
		ActorDeletedHandle = GEngine->OnLevelActorDeleted().AddUObject(this, &UArcTownDebugDrawSubsystem::HandleLevelActorChanged);
		ActorListChangedHandle = GEngine->OnLevelActorListChanged().AddUObject(this, &UArcTownDebugDrawSubsystem::HandleLevelActorListChanged);
	}
#endif
}

void UArcTownDebugDrawSubsystem::Deinitialize()
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
	CachedAttractors.Reset();
	FrozenCache.Reset();
	GeometryCache.Reset();
	ArcTownDebugDraw::ResetLogLatches();

	Super::Deinitialize();
}

TStatId UArcTownDebugDrawSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArcTownDebugDrawSubsystem, STATGROUP_Tickables);
}

void UArcTownDebugDrawSubsystem::HandleLevelChanged(ULevel* /*Level*/, UWorld* InWorld)
{
	if (InWorld == GetWorld())
	{
		MarkActorCacheDirty();
	}
}

#if WITH_EDITOR
void UArcTownDebugDrawSubsystem::HandleLevelActorChanged(AActor* Actor)
{
	if (Actor != nullptr && Actor->GetWorld() == GetWorld())
	{
		MarkActorCacheDirty();
	}
}

void UArcTownDebugDrawSubsystem::HandleLevelActorListChanged()
{
	MarkActorCacheDirty();
}
#endif

void UArcTownDebugDrawSubsystem::MarkActorCacheDirty()
{
	bActorCacheDirty = true;
}

void UArcTownDebugDrawSubsystem::RefreshActorCache(UWorld& World)
{
	CachedVolumes.Reset();
	for (TActorIterator<AArcTownVolume> VolumeIt(&World); VolumeIt; ++VolumeIt)
	{
		if (IsValid(*VolumeIt))
		{
			CachedVolumes.Emplace(*VolumeIt);
		}
	}

	CachedAttractors.Reset();
	for (TActorIterator<AArcTownAttractorVolume> AttractorIt(&World); AttractorIt; ++AttractorIt)
	{
		if (IsValid(*AttractorIt))
		{
			CachedAttractors.Emplace(*AttractorIt);
		}
	}

	// A cached entry holds a whole frozen site snapshot, and the draw path never revisits a deleted volume.
	TSet<TWeakObjectPtr<const AArcTownVolume>> LiveVolumes;
	LiveVolumes.Reserve(CachedVolumes.Num());
	for (const TWeakObjectPtr<AArcTownVolume>& VolumePtr : CachedVolumes)
	{
		if (const AArcTownVolume* const Volume = VolumePtr.Get())
		{
			LiveVolumes.Emplace(Volume);
		}
	}
	FrozenCache.Prune(LiveVolumes);
	GeometryCache.Prune(LiveVolumes);

	bActorCacheDirty = false;
}

void UArcTownDebugDrawSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	if (bActorCacheDirty)
	{
		RefreshActorCache(*World);
	}

	bool bAnyEnabled = false;
	for (const TWeakObjectPtr<AArcTownVolume>& VolumePtr : CachedVolumes)
	{
		const AArcTownVolume* const Volume = VolumePtr.Get();
		if (Volume != nullptr && Volume->HasDebugOverlayEnabled())
		{
			bAnyEnabled = true;
			break;
		}
	}

	if (!bAnyEnabled)
	{
		return;
	}

	for (const TWeakObjectPtr<AArcTownVolume>& VolumePtr : CachedVolumes)
	{
		const AArcTownVolume* const Volume = VolumePtr.Get();
		if (Volume != nullptr && Volume->HasDebugOverlayEnabled())
		{
			ArcTownDebugDraw::DrawVolume(*Volume, CachedAttractors, FrozenCache, GeometryCache);
		}
	}
}
