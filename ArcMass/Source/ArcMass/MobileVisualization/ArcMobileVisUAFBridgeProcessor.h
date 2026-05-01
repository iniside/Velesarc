// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "ArcMobileVisUAFBridgeProcessor.generated.h"

UCLASS()
class ARCMASS_API UArcMobileVisUAFBridgeProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcMobileVisUAFBridgeProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
