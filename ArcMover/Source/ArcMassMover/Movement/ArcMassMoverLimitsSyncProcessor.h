// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/EntityQuery.h"
#include "Mass/Processor.h"

#include "ArcMassMoverLimitsSyncProcessor.generated.h"

UCLASS()
class ARCMASSMOVER_API UArcMassMoverLimitsSyncProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcMassMoverLimitsSyncProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery{*this};
};
