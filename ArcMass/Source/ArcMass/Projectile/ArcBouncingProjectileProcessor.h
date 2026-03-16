// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"

#include "ArcBouncingProjectileProcessor.generated.h"

UCLASS()
class ARCMASS_API UArcBouncingProjectileProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcBouncingProjectileProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
