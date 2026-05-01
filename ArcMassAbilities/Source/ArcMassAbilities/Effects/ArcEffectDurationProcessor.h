// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "ArcEffectDurationProcessor.generated.h"

UCLASS()
class ARCMASSABILITIES_API UArcEffectDurationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcEffectDurationProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
