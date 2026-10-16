// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/Processor.h"
#include "ArcTabTargetRetentionProcessor.generated.h"

UCLASS(MinimalAPI)
class UArcTabTargetRetentionProcessor : public UMassProcessor
{
	GENERATED_BODY()

protected:
	UArcTabTargetRetentionProcessor();

	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery{*this};
};
