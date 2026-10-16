// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/Processor.h"

#include "ArcGatherSchedulerProcessor.generated.h"

UCLASS()
class ARCWORLDSIM_API UArcGatherSchedulerProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcGatherSchedulerProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery BodyQuery;
	FMassEntityQuery AreaQuery;
};
