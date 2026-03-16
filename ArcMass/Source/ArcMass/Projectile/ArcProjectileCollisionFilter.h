// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CollisionQueryFilterCallbackCore.h"

class AActor;
/**
 * Chaos collision filter that blocks all hits EXCEPT actors in the ignore list.
 * Used per-entity by the projectile movement processor.
 *
 * Call SetIgnoredActors() before each entity's queries. The pointer is NOT owned.
 */
class FArcProjectileCollisionFilterCallback : public ICollisionQueryFilterCallback
{
public:
	/** Set the ignore list for the current entity. Pointer must remain valid during queries. */
	void SetIgnoredActors(const TArray<TWeakObjectPtr<AActor>>* InIgnoredActors)
	{
		IgnoredActors = InIgnoredActors;
	}

	virtual ECollisionQueryHitType PreFilter(const FQueryFilterData& FilterData, const Chaos::FPerShapeData& Shape, const Chaos::FGeometryParticle& Actor) override;

	// PT overload — runs on physics thread; we're game-thread only so just block all.
	virtual ECollisionQueryHitType PreFilter(const FQueryFilterData& FilterData, const Chaos::FPerShapeData& Shape, const Chaos::FGeometryParticleHandle& Actor) override
	{
		return ECollisionQueryHitType::Block;
	}

	virtual ECollisionQueryHitType PostFilter(const FQueryFilterData& FilterData, const ChaosInterface::FQueryHit& Hit) override
	{
		return ECollisionQueryHitType::Block;
	}

	virtual ECollisionQueryHitType PostFilter(const FQueryFilterData& FilterData, const ChaosInterface::FPTQueryHit& Hit) override
	{
		return ECollisionQueryHitType::Block;
	}

private:
	const TArray<TWeakObjectPtr<AActor>>* IgnoredActors = nullptr;
};
