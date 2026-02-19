// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassSignalProcessorBase.h"

#include "ArcBiologicalConditionProcessor.generated.h"

// ---------------------------------------------------------------------------
// Biological Application Processor
//
// Handles application of: Bleeding, Poisoned, Diseased, Weakened
// Cross-condition interactions:
//   - Bleeding blocked by active Burning (cauterization) and Frozen
//   - Poison absorption boost from Bleeding (+50% accumulation)
//   - Disease passive spread (future)
// ---------------------------------------------------------------------------

UCLASS()
class ARCCONDITIONEFFECTS_API UArcBiologicalConditionProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcBiologicalConditionProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};
