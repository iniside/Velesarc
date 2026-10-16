// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/Processor.h"

#include "ArcProductionProcessor.generated.h"

UCLASS()
class ARCWORLDSIM_API UArcProductionProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcProductionProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
	int32 CompletedCrafts = 0;
};
