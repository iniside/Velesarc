// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/Processor.h"
#include "Mass/EntityQuery.h"

#include "ArcCueLostTargetProcessor.generated.h"

UCLASS()
class ARCCUES_API UArcCueLostTargetProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcCueLostTargetProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery{*this};
};
