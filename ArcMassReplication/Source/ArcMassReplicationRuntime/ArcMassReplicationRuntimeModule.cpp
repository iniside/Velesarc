// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassReplicationRuntimeModule.h"

#include "Iris/ReplicationSystem/NetObjectFactoryRegistry.h"
#include "Modules/ModuleManager.h"
#include "Replication/ArcMassEntityRootFactory.h"

#define LOCTEXT_NAMESPACE "FArcMassReplicationRuntimeModule"

void FArcMassReplicationRuntimeModule::StartupModule()
{
	// Iris factory IDs are registration-slot indices (FNetObjectFactoryRegistry): the Nth
	// RegisterFactory call gets id N, and creation headers encode that id so the client looks the
	// factory up by the same number the server stamped. Server and client MUST register every
	// factory in identical order, or headers dispatch to the wrong factory. Registration must also
	// complete before the first ReplicationSystem exists (later calls check() — SetFactoryRegistrationAllowed).
	//
	// Engine-core factories (Actor/SubObject/RootObject) register first and deterministically at
	// EDelayedRegisterRunPhase::ObjectSystemReady (UnrealEngine.cpp), before any Default-phase
	// module loads — no divergence risk there. The two OTHER factory-registering modules live in
	// this project both register at LoadingPhase::Default with no dependency edge to us:
	//   - Entity (EntityFramework plugin) -> UNetComponentFactory, UNetEntityFactory
	//   - MassIrisReplication            -> UMassNetObjectFactory
	// As Default-phase siblings their load order relative to ArcMassReplicationRuntime is not
	// guaranteed and can differ between a dedicated-server and a client/editor target (different
	// enabled-module sets shift the plugin manager's tie-breaking). Force-load them here, in a
	// fixed order, BEFORE our RegisterFactory so UArcMassEntityRootFactory always occupies a stable
	// slot after them on both server and client. Both are Type=Runtime and unconditionally enabled
	// in ArcGame.uproject, so they are present in every target and LoadModuleChecked cannot fail.
	// (MassReplication registers no Iris factory in this engine version; Epic's legacy
	// MassBubbleNetObjectFactory reference in MassIrisReplicationModule.cpp does not apply here.)
	// Mirrors MassIrisReplicationModule.cpp's LoadModuleChecked-before-RegisterFactory pattern.
	FModuleManager& ModuleManager = FModuleManager::Get();
	ModuleManager.LoadModuleChecked(TEXT("Entity"));
	ModuleManager.LoadModuleChecked(TEXT("MassIrisReplication"));

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
