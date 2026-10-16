// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/Processor.h"
#include "ArcMobileVisActorTransformPushProcessor.generated.h"

UCLASS()
class ARCMASS_API UArcMobileVisActorTransformPushProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcMobileVisActorTransformPushProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
