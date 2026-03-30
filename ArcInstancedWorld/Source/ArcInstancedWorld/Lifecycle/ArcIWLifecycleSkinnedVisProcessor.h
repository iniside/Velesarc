// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassSignalProcessorBase.h"
#include "ArcIWLifecycleSkinnedVisProcessor.generated.h"

UCLASS()
class ARCINSTANCEDWORLD_API UArcIWLifecycleSkinnedVisProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcIWLifecycleSkinnedVisProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};
