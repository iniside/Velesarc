// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/EntityQuery.h"
#include "Mass/Processor.h"

#include "ArcPSOPrecacheRebuildProcessor.generated.h"

struct FMassEntityManager;
struct FMassExecutionContext;

UCLASS()
class ARCMASSRENDER_API UArcPSOPrecacheRebuildProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcPSOPrecacheRebuildProcessor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& ExecutionContext) override;

	FMassEntityQuery EntityQuery;
};
