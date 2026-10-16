// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UReplicationSystem;

namespace UE::Net
{
	struct FNetObjectResolveContext;
}

namespace ArcMassIrisWorldResolver
{
	// Resolve the world tied to a specific UReplicationSystem by matching it against
	// each world context's NetDriver. This is the correct way to scope per-world
	// state when running multiple worlds (PIE host + client share a process); a
	// best-effort first-Game-world walk causes observer reentrancy by landing
	// client-spawned local entities in the server world.
	UWorld* FindWorldForReplicationSystem(const UReplicationSystem* Target);

	// Convenience overload for callers that only have an FNetObjectResolveContext
	// (e.g. factory InstantiateContext). Null-guards ResolveContext.ReplicationSystem.
	UWorld* FindWorldForReplicationSystem(const UE::Net::FNetObjectResolveContext& ResolveContext);
}
