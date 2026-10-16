// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/Processor.h"

#include "ArcTransportJobProcessor.generated.h"

UCLASS()
class ARCWORLDSIM_API UArcTransportJobProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcTransportJobProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery BodyQuery;
	FMassEntityQuery StorageQuery;
};
