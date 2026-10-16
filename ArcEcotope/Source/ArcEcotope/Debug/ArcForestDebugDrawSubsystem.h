// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Subsystems/WorldSubsystem.h"

#include "Debug/ArcForestDebugDrawTypes.h"

#include "ArcForestDebugDrawSubsystem.generated.h"

class AActor;
class AArcClimateVolume;
class AArcEcotopeInstancePartitionActor;
class AArcEcotopeVolume;
class ULevel;

UCLASS()
class ARCECOTOPE_API UArcForestDebugDrawSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem / FTickableGameObject interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool IsTickableInEditor() const override { return true; }
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~ End USubsystem / FTickableGameObject interface

private:
	void HandleLevelChanged(ULevel* Level, UWorld* InWorld);
#if WITH_EDITOR
	void HandleLevelActorChanged(AActor* Actor);
	void HandleLevelActorListChanged();
#endif

	void MarkActorCacheDirty();
	void RefreshActorCache(UWorld& World);

	TArray<TWeakObjectPtr<AArcEcotopeVolume>> CachedVolumes;
	TArray<TWeakObjectPtr<AArcClimateVolume>> CachedClimateVolumes;
	TArray<TWeakObjectPtr<AArcEcotopeInstancePartitionActor>> CachedInstancePartitionActors;
	bool bActorCacheDirty = true;

	FArcForestDebugDrawSiteReaderCache SiteReaderCache;
	FArcForestDebugDrawGeometryCache GeometryCache;

	FDelegateHandle LevelAddedHandle;
	FDelegateHandle LevelRemovedHandle;

#if WITH_EDITOR
	FDelegateHandle ActorAddedHandle;
	FDelegateHandle ActorDeletedHandle;
	FDelegateHandle ActorListChangedHandle;
#endif
};
