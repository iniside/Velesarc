// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ArcIWSettings.generated.h"

class UTransformProviderData;

/**
 * Project-wide settings for ArcInstancedWorld.
 * Accessible in editor via Project Settings > Game > ArcInstancedWorld.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "ArcInstancedWorld"))
class ARCINSTANCEDWORLD_API UArcIWSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UArcIWSettings();

	//~ UDeveloperSettings interface
	virtual FName GetCategoryName() const override { return FName(TEXT("Game")); }
	//~ End of UDeveloperSettings interface

	static const UArcIWSettings* Get() { return GetDefault<UArcIWSettings>(); }

	/** Default World Partition runtime grid name for new ArcIW partition actors.
	 *  Can be overridden per-actor on UArcIWMassConfigComponent. */
	UPROPERTY(EditAnywhere, config, Category = "ArcInstancedWorld")
	FName DefaultGridName = TEXT("MainGrid");

	/** Fallback grid cell size (in cm) used when the WP streaming grid is not available.
	 *  This is used during editor conversion if RuntimeHash hasn't been generated yet. */
	UPROPERTY(EditAnywhere, config, Category = "ArcInstancedWorld", meta = (ClampMin = "100"))
	uint32 DefaultGridCellSize = 12800;

	/** When true, traces hitting ArcIW ISM components will automatically hydrate
	 *  the hit entity into a full actor. Disabled by default. */
	UPROPERTY(EditAnywhere, config, Category = "ArcInstancedWorld")
	bool bHydrateOnHit = false;

	/** Use MassISM instead of ISM for instanced representation. Experimental.
	 *  Requires editor re-conversion of partition actors to take effect. */
	UPROPERTY(EditAnywhere, config, Category = "ArcInstancedWorld|MassISM")
	bool bUseMassISM = false;

	/** When true (and bUseMassISM is true), skip actor hydration entirely —
	 *  entities stay in MassISM representation regardless of distance. */
	UPROPERTY(EditAnywhere, config, Category = "ArcInstancedWorld|MassISM", meta = (EditCondition = "bUseMassISM"))
	bool bDisableActorHydration = false;

	/** Default transform provider for skinned meshes that don't have one set explicitly.
	 *  Used as the final fallback in the override chain. */
	UPROPERTY(EditAnywhere, config, Category = "ArcInstancedWorld|SkinnedMesh")
	TSoftObjectPtr<UTransformProviderData> DefaultSkinnedMeshTransformProvider;

	/** MassISM: ISM instances appear at this distance from player. */
	UPROPERTY(EditAnywhere, config, Category = "ArcInstancedWorld|MassISM", meta = (EditCondition = "bUseMassISM", ClampMin = "0"))
	float MeshAddRadius = 20000.f;

	/** MassISM: ISM instances removed beyond this distance. Must be > MeshAddRadius. */
	UPROPERTY(EditAnywhere, config, Category = "ArcInstancedWorld|MassISM", meta = (EditCondition = "bUseMassISM", ClampMin = "0"))
	float MeshRemoveRadius = 25000.f;

	/** MassISM: Physics bodies created at this distance from player. */
	UPROPERTY(EditAnywhere, config, Category = "ArcInstancedWorld|MassISM", meta = (EditCondition = "bUseMassISM", ClampMin = "0"))
	float PhysicsAddRadius = 12000.f;

	/** MassISM: Physics bodies removed beyond this distance. Must be > PhysicsAddRadius. */
	UPROPERTY(EditAnywhere, config, Category = "ArcInstancedWorld|MassISM", meta = (EditCondition = "bUseMassISM", ClampMin = "0"))
	float PhysicsRemoveRadius = 15000.f;

	/** MassISM: Spatial hash cell size for ISM mesh instance grid (cm). */
	UPROPERTY(EditAnywhere, config, Category = "ArcInstancedWorld|MassISM", meta = (EditCondition = "bUseMassISM", ClampMin = "100"))
	float MeshCellSize = 10000.f;

	/** MassISM: Spatial hash cell size for physics body grid (cm). */
	UPROPERTY(EditAnywhere, config, Category = "ArcInstancedWorld|MassISM", meta = (EditCondition = "bUseMassISM", ClampMin = "100"))
	float PhysicsCellSize = 8000.f;

	/** MassISM: Spatial hash cell size for actor hydration grid (cm). */
	UPROPERTY(EditAnywhere, config, Category = "ArcInstancedWorld|MassISM", meta = (EditCondition = "bUseMassISM", ClampMin = "100"))
	float ActorCellSize = 5000.f;

	/** MassISM: Entities within this distance get full actor representation. */
	UPROPERTY(EditAnywhere, config, Category = "ArcInstancedWorld|MassISM", meta = (EditCondition = "bUseMassISM", ClampMin = "0"))
	float ActorHydrationRadius = 3000.f;

	/** MassISM: Hydrated actors beyond this distance return to ISM.
	 *  Should be >= ActorHydrationRadius to prevent thrashing. */
	UPROPERTY(EditAnywhere, config, Category = "ArcInstancedWorld|MassISM", meta = (EditCondition = "bUseMassISM", ClampMin = "0"))
	float ActorDehydrationRadius = 8000.f;

};
