// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/Processor.h"

#include "ArcLedgerProcessor.generated.h"

UCLASS()
class ARCWORLDSIM_API UArcLedgerProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcLedgerProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ProductionQuery;
	FMassEntityQuery OutputHaveQuery;
	FMassEntityQuery StorageQuery;
	FMassEntityQuery ItemSourceQuery;
	FMassEntityQuery TransportJobQuery;
	FMassEntityQuery StrandedCargoQuery;
	FMassEntityQuery ItemConsumerQuery;
};
