// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"

#include "ArcConditionTickProcessors.generated.h"

UCLASS()
class ARCCONDITIONEFFECTS_API UArcConditionTickProcessor : public UMassProcessor
{
    GENERATED_BODY()

public:
    UArcConditionTickProcessor();

protected:
    virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
    FMassEntityQuery EntityQuery;
};
