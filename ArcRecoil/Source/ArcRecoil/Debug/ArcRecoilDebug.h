// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/Processor.h"

#include "ArcRecoilDebug.generated.h"

UCLASS()
class UArcRecoilDebugProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcRecoilDebugProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
