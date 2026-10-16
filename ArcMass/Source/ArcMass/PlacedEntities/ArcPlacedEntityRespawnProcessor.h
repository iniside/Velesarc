// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/Processor.h"
#include "Mass/EntityQuery.h"
#include "ArcPlacedEntityRespawnProcessor.generated.h"

UCLASS()
class ARCMASS_API UArcPlacedEntityRespawnProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcPlacedEntityRespawnProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery TrackerQuery{*this};

	/**
	 * Hand-rolled 1Hz throttle. Engine has no per-processor TickInterval — confirmed by research.
	 * Reset to 0 on hot-reload (CDO is rebuilt); first sweep then fires at most SweepInterval seconds later.
	 */
	float AccumulatedTime = 0.0f;

	static constexpr float SweepInterval = 1.0f;
};
