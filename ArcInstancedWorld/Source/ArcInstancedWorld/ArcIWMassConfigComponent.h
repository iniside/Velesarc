// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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
};
