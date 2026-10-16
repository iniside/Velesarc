// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Subsystems/WorldSubsystem.h"

#include "Debug/ArcTownDebugDrawTypes.h"

#include "ArcTownDebugDrawSubsystem.generated.h"

class AActor;
class AArcTownAttractorVolume;
class AArcTownVolume;
class ULevel;

UCLASS()
class ARCTOWN_API UArcTownDebugDrawSubsystem : public UTickableWorldSubsystem
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

	TArray<TWeakObjectPtr<AArcTownVolume>> CachedVolumes;
	TArray<TWeakObjectPtr<const AArcTownAttractorVolume>> CachedAttractors;
	bool bActorCacheDirty = true;

	FArcTownDebugDrawFrozenCache FrozenCache;
	FArcTownDebugDrawGeometryCache GeometryCache;

	FDelegateHandle LevelAddedHandle;
	FDelegateHandle LevelRemovedHandle;

#if WITH_EDITOR
	FDelegateHandle ActorAddedHandle;
	FDelegateHandle ActorDeletedHandle;
	FDelegateHandle ActorListChangedHandle;
#endif
};
