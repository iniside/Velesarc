// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/EntityQuery.h"
#include "Mass/Processor.h"

#include "ArcMassApplyForceProcessor.generated.h"

UCLASS()
class ARCMASSGAMEPLAYFRAMEWORK_API UArcMassApplyForceProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcMassApplyForceProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery{*this};
};
