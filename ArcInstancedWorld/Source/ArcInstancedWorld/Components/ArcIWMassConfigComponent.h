// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArcMass/Physics/ArcMassPhysicsBody.h"
#include "ArcIWMassConfigComponent.generated.h"

class UMassEntityConfigAsset;

/**
 * Place this component on an actor to provide additional Mass entity config
 * that will be merged into the entity template when converting to ArcIW partition.
 */
UCLASS(ClassGroup = (ArcIW), meta = (BlueprintSpawnableComponent))
class ARCINSTANCEDWORLD_API UArcIWMassConfigComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArcIWMassConfigComponent();
	/** Mass entity config whose traits will be merged into the entity archetype. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ArcIW")
	TObjectPtr<UMassEntityConfigAsset> EntityConfig = nullptr;

	/** World Partition runtime grid name. The partition actor cell size is taken from this grid.
	 *  Must match a grid name configured in the World Partition Runtime Spatial Hash settings.
	 *  Default value comes from Project Settings > Game > ArcInstancedWorld. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ArcIW")
	FName RuntimeGridName;

	/** Time in seconds before a removed instance respawns. 0 = never respawn (permanent removal). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ArcIW")
	int32 RespawnTimeSeconds = 0;

	/** Controls how the physics body is created. Static = trace-only, Dynamic = can respond to forces. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ArcIW")
	EArcMassPhysicsBodyType PhysicsBodyType = EArcMassPhysicsBodyType::Static;
};
