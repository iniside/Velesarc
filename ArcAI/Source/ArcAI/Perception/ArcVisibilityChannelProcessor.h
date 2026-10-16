// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/Processor.h"

#include "ArcVisibilityChannelProcessor.generated.h"

/** One perceiver's view shape, snapshotted at cycle start so the rebuild is unaffected by later movement or despawns. */
struct FArcVisibilityCaster
{
	FVector Origin = FVector::ZeroVector;
	/** Normalized 2D facing, zero when the perceiver looks straight up or down. */
	FVector2D Forward = FVector2D::ZeroVector;
	double Range = 0.0;
	double CosHalfAngle = -1.0;
	double HalfAngleRadians = PI;
	int32 TeamId = 0;
};

/**
 * Rebuilds the per-team visibility bitplanes of every tiled influence grid, one budget cycle at a time:
 * clear every back plane, shadowcast every perceiver into it over as many ticks as the budget needs,
 * then publish. Readers only ever see a whole cycle's result.
 */
UCLASS()
class ARCAI_API UArcVisibilityChannelProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcVisibilityChannelProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery PerceiverQuery;

	TArray<FArcVisibilityCaster> Casters;
	int32 CasterCursor = 0;
	bool bCycleActive = false;
};
