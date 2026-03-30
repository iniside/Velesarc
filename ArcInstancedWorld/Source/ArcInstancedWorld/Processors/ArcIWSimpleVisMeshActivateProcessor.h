// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassSignalProcessorBase.h"
#include "ArcIWSimpleVisMeshActivateProcessor.generated.h"

UCLASS()
class ARCINSTANCEDWORLD_API UArcIWSimpleVisMeshActivateProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcIWSimpleVisMeshActivateProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;

	FMassEntityQuery SkinnedEntityQuery{*this};
};
