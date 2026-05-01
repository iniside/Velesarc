// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassSignalProcessorBase.h"
#include "MassEntityQuery.h"
#include "ArcTransportManagerProcessor.generated.h"

UCLASS()
class ARCECONOMY_API UArcTransportManagerProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcTransportManagerProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;

private:
	FMassEntityQuery TransporterQuery;
	FMassEntityQuery BuildingQuery;
	FMassEntityQuery ConsumerBuildingQuery;
};
