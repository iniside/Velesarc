// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/Processor.h"

#include "ArcGatheringProcessor.generated.h"

UCLASS()
class ARCWORLDSIM_API UArcGatheringProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcGatheringProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
