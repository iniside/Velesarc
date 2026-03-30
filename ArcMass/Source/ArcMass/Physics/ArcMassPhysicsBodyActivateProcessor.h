// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassSignalProcessorBase.h"
#include "ArcMassPhysicsBodyActivateProcessor.generated.h"

UCLASS()
class ARCMASS_API UArcMassPhysicsBodyActivateProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcMassPhysicsBodyActivateProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};
