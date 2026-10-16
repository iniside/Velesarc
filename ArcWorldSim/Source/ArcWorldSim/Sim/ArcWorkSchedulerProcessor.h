// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/Processor.h"

#include "ArcWorkSchedulerProcessor.generated.h"

UCLASS()
class ARCWORLDSIM_API UArcWorkSchedulerProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcWorkSchedulerProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery BodyQuery;
};
