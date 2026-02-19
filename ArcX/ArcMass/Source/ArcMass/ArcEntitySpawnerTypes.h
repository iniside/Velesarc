// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityManager.h"
#include "ArcEntitySpawnerTypes.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogArcEntitySpawner, Log, All);

/**
 * Base class for post-spawn entity initializers.
 * Subclass in C++ or Blueprint to customize entities after spawning.
 * Initializers run while the EntityCreationContext is alive — observers haven't fired yet.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class ARCMASS_API UArcEntityInitializer : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Called to initialize a batch of freshly spawned entities.
	 * Override in C++ for direct fragment access, or implement BP_InitializeEntities in Blueprint.
	 */
	virtual void InitializeEntities(FMassEntityManager& EntityManager, TArrayView<const FMassEntityHandle> Entities);

protected:
	/** Blueprint override point for entity initialization. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Mass|Spawn", meta = (DisplayName = "Initialize Entities"))
	void BP_InitializeEntities(const TArray<FMassEntityHandle>& Entities);
};
