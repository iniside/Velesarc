// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/Processor.h"

#include "ArcTargetedProjectileProcessor.generated.h"

UCLASS()
class ARCMASS_API UArcTargetedProjectileProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcTargetedProjectileProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery{*this};
};
