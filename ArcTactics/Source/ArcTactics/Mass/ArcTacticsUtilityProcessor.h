// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Actions/ArcTacticsAction.h"
#include "Mass/ArcTacticsInstanceHandle.h"
#include "MassSignalProcessorBase.h"

#include "ArcTacticsUtilityProcessor.generated.h"

class UArcTacticsInstanceSubsystem;

UCLASS()
class ARCTACTICS_API UArcTacticsUtilityProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcTacticsUtilityProcessor();

	static void RunDecision(
		UArcTacticsInstanceSubsystem& Instances,
		FMassEntityManager& EntityManager,
		FArcTacticsInstanceHandle Handle,
		float DeltaTime,
		const ArcPathFollow::FContext* PathContext);
	static void TickAction(
		UArcTacticsInstanceSubsystem& Instances,
		FMassEntityManager& EntityManager,
		FArcTacticsInstanceHandle Handle,
		EArcTacticsActionTickLane Lane,
		bool bExternalWake,
		const ArcPathFollow::FContext* PathContext);

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(
		FMassEntityManager& EntityManager,
		FMassExecutionContext& Context,
		FMassSignalNameLookup& EntitySignals) override;
};
