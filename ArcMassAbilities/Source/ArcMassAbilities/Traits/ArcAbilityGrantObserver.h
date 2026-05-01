// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassObserverProcessor.h"
#include "ArcAbilityGrantObserver.generated.h"

UCLASS()
class ARCMASSABILITIES_API UArcAbilityGrantObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcAbilityGrantObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};
