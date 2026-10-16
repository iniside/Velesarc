// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/Processor.h"

#include "ArcGovernorProcessor.generated.h"

UCLASS()
class ARCWORLDSIM_API UArcGovernorProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcGovernorProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery CenterQuery;
	FMassEntityQuery BodyQuery;
	FMassEntityQuery EmploymentQuery;
	FMassEntityQuery ItemSourceQuery;
	FMassEntityQuery StorageQuery;
};
