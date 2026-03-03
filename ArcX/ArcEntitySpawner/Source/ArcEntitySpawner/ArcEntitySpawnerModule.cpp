// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEntitySpawnerModule.h"

#include "ArcEntitySpawner.h"
#include "ArcPersistenceClassRegistry.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FArcEntitySpawnerModule, ArcEntitySpawner)

void FArcEntitySpawnerModule::StartupModule()
{
	ArcPersistence::RegisterPersistentClass(AArcEntitySpawner::StaticClass(), /*bIncludeChildren=*/ false);
}

void FArcEntitySpawnerModule::ShutdownModule()
{
	ArcPersistence::UnregisterPersistentClass(AArcEntitySpawner::StaticClass());
}
