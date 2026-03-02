// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEntitySpawnerTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcEntitySpawnerTypes)

DEFINE_LOG_CATEGORY(LogArcEntitySpawner);

//----------------------------------------------------------------------
// UArcEntityInitializer
//----------------------------------------------------------------------

void UArcEntityInitializer::InitializeEntities(FMassEntityManager& EntityManager, TArrayView<const FMassEntityHandle> Entities)
{
	// Default implementation calls Blueprint event
	TArray<FMassEntityHandle> EntitiesArray(Entities.GetData(), Entities.Num());
	BP_InitializeEntities(EntitiesArray);
}
