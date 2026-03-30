// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityHandle.h"

struct FBodyInstance;
struct FHitResult;
struct FChaosUserEntityAppend;

/**
 * Links FMassEntityHandle to Chaos physics bodies on ISM instances.
 *
 * When a visualization system creates ISM instances for Mass entities,
 * it can call Attach() to inject the entity handle into the Chaos particle's
 * user data. The engine's existing extraction helpers (GetUserData, etc.)
 * continue to find FBodyInstance through ChaosUserEntityAppend->ChaosUserData.
 *
 * All functions must be called on the game thread.
 */
namespace ArcMassPhysicsEntityLink
{
	/**
	 * Attach an entity handle to a physics body's Chaos particle.
	 * Wraps the body's existing user data in FChaosUserEntityAppend so engine
	 * code still resolves FBodyInstance normally.
	 *
	 * @param Body           The ISM instance's FBodyInstance (from InstanceBodies[i]).
	 * @param EntityHandle   The Mass entity that owns this ISM instance.
	 * @param OwnerObject    UObject whose lifetime governs the physics body (typically the ISM component).
	 * @return Allocated append pointer. Caller owns the lifetime — pass to Detach() before the body is destroyed.
	 *         Returns nullptr if the body has no valid physics actor.
	 */
	ARCMASS_API FChaosUserEntityAppend* Attach(FBodyInstance& Body, FMassEntityHandle EntityHandle, UObject* OwnerObject);

	/**
	 * Remove entity link from a physics body, restore original user data, and free allocations.
	 * Safe to call with nullptr Append (no-op).
	 *
	 * @param Body    The same FBodyInstance passed to Attach().
	 * @param Append  The pointer returned by Attach(). Will be deleted.
	 */
	ARCMASS_API void Detach(FBodyInstance& Body, FChaosUserEntityAppend* Append);

	/**
	 * Extract FMassEntityHandle from a trace hit result.
	 * Tries the component-based path first (ISM instances with owning component),
	 * then falls back to ResolveHitFromPhysicsObject for componentless bodies.
	 * Returns an invalid handle if the hit body has no entity link attached.
	 */
	ARCMASS_API FMassEntityHandle ResolveHit(const FHitResult& HitResult);

	/**
	 * Resolve hit to entity via Chaos physics object - no component required.
	 * Works with standalone FBodyInstance (null owning component).
	 * Goes from FHitResult::PhysicsObject -> proxy -> particle -> UserData -> FChaosUserEntityAppend -> FMassEntityHandle.
	 */
	ARCMASS_API FMassEntityHandle ResolveHitFromPhysicsObject(const FHitResult& HitResult);

	/**
	 * Extract FBodyInstance from a trace hit via Chaos physics object.
	 * Works with standalone bodies (no UPrimitiveComponent required).
	 * Returns nullptr if no body is found.
	 */
	ARCMASS_API FBodyInstance* ResolveHitToBody(const FHitResult& HitResult);
}
