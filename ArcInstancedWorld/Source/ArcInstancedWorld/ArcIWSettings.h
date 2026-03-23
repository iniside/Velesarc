// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ArcIWSettings.generated.h"

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

	/** Visualization grid cell size in world units (cm). */
	UPROPERTY(EditAnywhere, config, Category = "ArcInstancedWorld|Grid", meta = (ClampMin = "100"))
	float CellSize = 6000.f;

	/** ISM swap radius — entities within this distance get ISM representation. */
	UPROPERTY(EditAnywhere, config, Category = "ArcInstancedWorld|Grid", meta = (ClampMin = "0"))
	float SwapRadius = 12000.f;

	/** Actor hydration radius — entities within this distance get full actor representation. */
	UPROPERTY(EditAnywhere, config, Category = "ArcInstancedWorld|Grid", meta = (ClampMin = "0"))
	float ActorRadius = 3000.f;

	/** Dehydration radius — hydrated actors beyond this distance return to ISM.
	 *  Should be >= ActorRadius to prevent hydrate/dehydrate thrashing. */
	UPROPERTY(EditAnywhere, config, Category = "ArcInstancedWorld|Grid", meta = (ClampMin = "0"))
	float DehydrationRadius = 8000.f;

};
