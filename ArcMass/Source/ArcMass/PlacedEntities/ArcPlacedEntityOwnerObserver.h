// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ObserverProcessor.h"
#include "ArcPlacedEntityOwnerObserver.generated.h"

/** Observer that notifies the owning partition actor when a placed entity's FArcPlacedEntityOwnerFragment is removed. */
UCLASS()
class ARCMASS_API UArcPlacedEntityOwnerObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcPlacedEntityOwnerObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};
