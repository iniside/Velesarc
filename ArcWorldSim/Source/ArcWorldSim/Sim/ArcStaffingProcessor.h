// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Mass/Processor.h"

#include "ArcStaffingProcessor.generated.h"

UCLASS()
class ARCWORLDSIM_API UArcStaffingProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcStaffingProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery BuildingQuery;
	FMassEntityQuery BodyQuery;
	TMap<FMassEntityHandle, int32> LastPostedStaffCount;
};
