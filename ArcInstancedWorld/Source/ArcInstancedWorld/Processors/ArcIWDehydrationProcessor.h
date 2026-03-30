// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "ArcIWDehydrationProcessor.generated.h"

// ---------------------------------------------------------------------------
// Dehydration Processor
// Runs periodically, dehydrates entities in actor representation that are
// outside the actor radius and past the minimum hydration duration.
// ---------------------------------------------------------------------------

UCLASS()
class ARCINSTANCEDWORLD_API UArcIWDehydrationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcIWDehydrationProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
