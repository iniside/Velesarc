// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcConditionTypes.h"
#include "MassSignalProcessorBase.h"

#include "ArcEnvironmentalConditionProcessor.generated.h"

// ---------------------------------------------------------------------------
// Environmental Application Processor
//
// Handles application of: Blinded, Suffocating, Exhausted, Corroded, Shocked
// These conditions are mostly independent — simple generic application.
// ---------------------------------------------------------------------------


UCLASS()
class ARCCONDITIONEFFECTS_API UArcEnvironmentalConditionProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcEnvironmentalConditionProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};
