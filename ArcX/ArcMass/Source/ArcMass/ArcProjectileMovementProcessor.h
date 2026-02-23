// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "MassProcessor.h"

#include "ArcProjectileMovementProcessor.generated.h"

// ---------------------------------------------------------------------------
// Per-Entity Fragment — mutable state for a projectile
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCMASS_API FArcProjectileFragment : public FMassFragment
{
	GENERATED_BODY()

	/** World-space movement direction (unit vector). */
	UPROPERTY(EditAnywhere)
	FVector Direction = FVector::ForwardVector;

	/** Movement speed in cm/s. */
	UPROPERTY(EditAnywhere)
	float Speed = 5000.f;

	/** Collision radius for sphere overlap check at destination. */
	UPROPERTY(EditAnywhere)
	float CollisionRadius = 10.f;

	/** Maximum distance the projectile can travel before it should be destroyed. */
	UPROPERTY(EditAnywhere)
	float MaxDistance = 50000.f;

	/** Accumulated distance traveled so far. */
	UPROPERTY()
	float DistanceTraveled = 0.f;

	/** True once the projectile has hit something or exceeded MaxDistance. */
	UPROPERTY()
	bool bPendingDestroy = false;
};

// ---------------------------------------------------------------------------
// Tag — archetype differentiator
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCMASS_API FArcProjectileTag : public FMassTag
{
	GENERATED_BODY()
};

// ---------------------------------------------------------------------------
// Processor — linear movement + Chaos direct raycast
// ---------------------------------------------------------------------------

/**
 * Moves projectile entities along their direction at their speed each frame.
 * Performs collision detection using the Chaos spatial acceleration structure
 * directly, bypassing UWorld trace wrappers for maximum throughput.
 */
UCLASS()
class ARCMASS_API UArcProjectileMovementProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcProjectileMovementProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
