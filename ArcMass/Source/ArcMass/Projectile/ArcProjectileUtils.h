// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Chaos/ParticleHandleFwd.h"
#include "Engine/HitResult.h"
#include "Mass/EntityHandle.h"

namespace UE::ArcMass::Projectile
{
	namespace Events
	{
		inline const FName ProjectileHit = TEXT("ProjectileHit");
	}

	// -----------------------------------------------------------------------
	// Pending hit notification — collected during movement, dispatched after
	// -----------------------------------------------------------------------

	struct FPendingHitNotification
	{
		FMassEntityHandle ProjectileEntity;
		AActor* VisualizationActor = nullptr;
		TWeakObjectPtr<UObject> Instigator;
		FHitResult HitResult;
		FVector Velocity = FVector::ZeroVector;
		FMassEntityHandle TargetEntity;
		bool bExpired = false;
	};

	// -----------------------------------------------------------------------
	// Shared utility functions
	// -----------------------------------------------------------------------

	/** Extract the owning actor and component from a Chaos geometry particle via user data. */
	ARCMASS_API void ResolveHitActorComponent(
		const Chaos::FGeometryParticle* Particle,
		AActor*& OutActor,
		UPrimitiveComponent*& OutComponent);

	/** Build an FHitResult from raycast hit data. Actor/component already resolved by caller. */
	ARCMASS_API FHitResult BuildRaycastHitResult(
		const FVector& HitPos,
		const FVector& HitNormal,
		float Distance,
		int32 FaceIndex,
		AActor* HitActor,
		UPrimitiveComponent* HitComponent,
		const FVector& TraceStart,
		const FVector& TraceEnd);

	/** Build an FHitResult from overlap hit data. Actor/component already resolved by caller. */
	ARCMASS_API FHitResult BuildOverlapHitResult(
		const FVector& OverlapPos,
		AActor* HitActor,
		UPrimitiveComponent* HitComponent,
		const FVector& TraceStart);

	/** Dispatch collected hit notifications to actor (IArcProjectileActorInterface) and instigator (IArcCollisionHitHandler). */
	ARCMASS_API void DispatchHitNotifications(const TArray<FPendingHitNotification>& PendingHits);
}
