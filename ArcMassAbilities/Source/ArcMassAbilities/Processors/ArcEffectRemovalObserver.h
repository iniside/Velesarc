// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ObserverProcessor.h"
#include "ArcEffectRemovalObserver.generated.h"

UCLASS()
class ARCMASSABILITIES_API UArcEffectRemovalObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcEffectRemovalObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery{*this};
};
