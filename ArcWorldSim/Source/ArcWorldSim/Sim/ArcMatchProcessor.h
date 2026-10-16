// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/Processor.h"

#include "ArcMatchProcessor.generated.h"

UCLASS()
class ARCWORLDSIM_API UArcMatchProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcMatchProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery CarrierQuery;
	FMassEntityQuery StorageQuery;
	FMassEntityQuery OutputBufferQuery;
};
