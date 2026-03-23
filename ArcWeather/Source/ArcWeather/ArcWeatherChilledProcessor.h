// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "ArcWeatherChilledProcessor.generated.h"

UCLASS(meta = (DisplayName = "Arc Weather Chilled Processor"))
class ARCWEATHER_API UArcWeatherChilledProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcWeatherChilledProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
