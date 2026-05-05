// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassReplicationRuntimeModule.h"

#include "Iris/ReplicationSystem/NetObjectFactoryRegistry.h"
#include "Replication/ArcMassEntityRootFactory.h"

#define LOCTEXT_NAMESPACE "FArcMassReplicationRuntimeModule"

void FArcMassReplicationRuntimeModule::StartupModule()
{
	// Iris factories must register before any ReplicationSystem is created.
	UE::Net::FNetObjectFactoryRegistry::RegisterFactory(
		UArcMassEntityRootFactory::StaticClass(),
		UArcMassEntityRootFactory::GetFactoryName());
}

void FArcMassReplicationRuntimeModule::ShutdownModule()
{
	UE::Net::FNetObjectFactoryRegistry::UnregisterFactory(UArcMassEntityRootFactory::GetFactoryName());
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArcMassReplicationRuntimeModule, ArcMassReplicationRuntime)
