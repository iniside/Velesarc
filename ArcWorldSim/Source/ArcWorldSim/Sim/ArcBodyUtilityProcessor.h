// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/Processor.h"
#include "MassSignalProcessorBase.h"

#include "ArcBodyUtilityProcessor.generated.h"

struct FArcUtilityScoringInstance;

UCLASS()
class ARCWORLDSIM_API UArcBodyUtilityProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcBodyUtilityProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(
		FMassEntityManager& EntityManager,
		FMassExecutionContext& Context,
		FMassSignalNameLookup& EntitySignals) override;

private:
	void OnScoringFinished(
		FArcUtilityScoringInstance& CompletedInstance,
		FMassEntityHandle Entity,
		uint32 SubmittedBodyCommandSerial);

	FMassEntityQuery ServiceQuery;
	FMassEntityQuery CenterQuery;
};
