// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassSignalProcessorBase.h"
#include "Mass/EntityQuery.h"
#include "ArcAttributeObserverProcessor.generated.h"

/**
 * Signal processor that dispatches OnObservedAttributeChanged to attribute handlers
 * when an attribute they declared a dependency on (via FArcAttributeHandler::GatherInputs)
 * has changed.
 *
 * Subscribes to UE::ArcMassAbilities::Signals::AttributeChanged and runs after
 * UArcAttributeAggregatorProcessor so handlers see fully-aggregated values.
 */
UCLASS()
class ARCMASSABILITIES_API UArcAttributeObserverProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcAttributeObserverProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager,
	                            FMassExecutionContext& Context,
	                            FMassSignalNameLookup& EntitySignals) override;
};
