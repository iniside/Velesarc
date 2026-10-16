// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/Processor.h"
#include "Mass/EntityQuery.h"
#include "ArcMassReplicationSourceTrackingProcessor.generated.h"

UCLASS()
class ARCMASSREPLICATIONRUNTIME_API UArcMassReplicationSourceTrackingProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcMassReplicationSourceTrackingProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery SourceQuery{*this};
};
