// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassSignalProcessorBase.h"
#include "ArcAttributeAggregatorProcessor.generated.h"

UCLASS()
class ARCMASSABILITIES_API UArcAttributeAggregatorProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcAttributeAggregatorProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};
