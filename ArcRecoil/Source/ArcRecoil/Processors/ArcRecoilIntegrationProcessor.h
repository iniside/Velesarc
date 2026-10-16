// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/Processor.h"

#include "ArcRecoilIntegrationProcessor.generated.h"

UCLASS()
class ARCRECOIL_API UArcRecoilIntegrationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcRecoilIntegrationProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
