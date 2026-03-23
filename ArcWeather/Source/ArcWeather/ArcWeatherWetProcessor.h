// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "ArcWeatherWetProcessor.generated.h"

UCLASS(meta = (DisplayName = "Arc Weather Wet Processor"))
class ARCWEATHER_API UArcWeatherWetProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcWeatherWetProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
