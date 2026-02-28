// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArcSettlementTypes.h"
#include "ArcRegionVolume.generated.h"

class USphereComponent;
class UArcRegionDefinition;

/**
 * Level-placed actor that defines a region's spatial bounds.
 * Registers with UArcSettlementSubsystem on BeginPlay, cleans up on EndPlay.
 *
 * Regions are optional spatial groupings of settlements. Settlements within
 * the sphere are automatically associated with this region.
 * Distance-based queries always work; regions add designer-controlled grouping.
 */
UCLASS()
class ARCSETTLEMENT_API AArcRegionVolume : public AActor
{
	GENERATED_BODY()

public:
	AArcRegionVolume();

	/** Region definition data asset — tags, display name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region")
	TObjectPtr<UArcRegionDefinition> Definition;

	/** Visual/spatial representation of the region bounds. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Region")
	TObjectPtr<USphereComponent> SphereComponent;

	/** The runtime handle assigned by the subsystem. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Region")
	FArcRegionHandle RegionHandle;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
