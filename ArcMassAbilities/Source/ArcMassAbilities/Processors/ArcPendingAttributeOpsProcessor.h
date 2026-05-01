// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassSignalProcessorBase.h"
#include "MassEntityQuery.h"
#include "ArcPendingAttributeOpsProcessor.generated.h"

UCLASS()
class ARCMASSABILITIES_API UArcPendingAttributeOpsProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcPendingAttributeOpsProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};
