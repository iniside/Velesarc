// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcEntitySpawnerTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcEntityInitializers.generated.h"

/**
 * Sets fragment values on spawned entities from an array of FInstancedStruct.
 * For each entity, iterates the fragment values and copies matching fragment data.
 */
UCLASS(DisplayName = "Set Fragment Values")
class ARCMASS_API UArcSetFragmentValuesInitializer : public UArcEntityInitializer
{
	GENERATED_BODY()

public:
	virtual void InitializeEntities(FMassEntityManager& EntityManager, TArrayView<const FMassEntityHandle> Entities) override;

protected:
	/** Fragment values to set on each spawned entity. Each entry should be a fragment struct type. */
	UPROPERTY(EditAnywhere, Category = "Mass|Spawn", meta = (BaseStruct = "/Script/MassEntity.MassFragment"))
	TArray<FInstancedStruct> FragmentValues;
};

/**
 * Adds Mass tags to spawned entities.
 * Tags are added directly while the creation context is alive.
 */
UCLASS(DisplayName = "Add Tags")
class ARCMASS_API UArcAddTagsInitializer : public UArcEntityInitializer
{
	GENERATED_BODY()

public:
	virtual void InitializeEntities(FMassEntityManager& EntityManager, TArrayView<const FMassEntityHandle> Entities) override;

protected:
	/** Tag struct types to add to each spawned entity (must derive from FMassTag). */
	UPROPERTY(EditAnywhere, Category = "Mass|Spawn", meta = (BaseStruct = "/Script/MassEntity.MassTag"))
	TArray<FInstancedStruct> Tags;
};
