// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"

#include "ArcProjectileMovementProcessor.generated.h"

/**
 * Moves projectile entities each frame: applies gravity, homing, collision detection
 * using Chaos direct spatial queries, bounce physics, and batch destruction.
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
