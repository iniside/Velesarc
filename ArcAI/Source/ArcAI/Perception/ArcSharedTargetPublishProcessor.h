// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/Processor.h"

#include "ArcSharedTargetPublishProcessor.generated.h"

/**
 * Publishes each perceiver's sight results into ArcKnowledge as its team's shared target records,
 * so allies that never saw the target can act on it.
 */
UCLASS()
class ARCAI_API UArcSharedTargetPublishProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcSharedTargetPublishProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
