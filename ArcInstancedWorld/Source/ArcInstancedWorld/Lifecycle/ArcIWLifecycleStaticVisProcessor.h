// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassSignalProcessorBase.h"
#include "ArcIWLifecycleStaticVisProcessor.generated.h"

UCLASS()
class ARCINSTANCEDWORLD_API UArcIWLifecycleStaticVisProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcIWLifecycleStaticVisProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};
