// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/Processor.h"

#include "ArcTacticsLifecycleProcessor.generated.h"

UCLASS()
class ARCTACTICS_API UArcTacticsLifecycleProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcTacticsLifecycleProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery{*this};
};
