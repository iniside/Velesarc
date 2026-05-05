// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassSignalProcessorBase.h"
#include "ArcMassQuickBarAutoSelectProcessor.generated.h"

UCLASS()
class ARCMASSITEMSRUNTIME_API UArcMassQuickBarAutoSelectProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcMassQuickBarAutoSelectProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};
