// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "ArcCooldownProcessor.generated.h"

UCLASS()
class ARCMASSABILITIES_API UArcCooldownProcessor : public UMassProcessor
{
    GENERATED_BODY()

public:
    UArcCooldownProcessor();

protected:
    virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
    FMassEntityQuery EntityQuery;
};
