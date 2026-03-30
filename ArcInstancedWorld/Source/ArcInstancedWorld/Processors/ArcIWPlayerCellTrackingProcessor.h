// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "ArcIWPlayerCellTrackingProcessor.generated.h"

// ---------------------------------------------------------------------------
// Player Cell Tracking Processor
// Iterates entities with FArcIWSourceEntityTag + FTransformFragment (streaming sources).
// For each source, detects cell changes and signals affected ArcIW entities.
// ---------------------------------------------------------------------------

UCLASS()
class ARCINSTANCEDWORLD_API UArcIWPlayerCellTrackingProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcIWPlayerCellTrackingProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	/** Must always run even when no source entities exist (falls back to player camera). */
	virtual bool ShouldAllowQueryBasedPruning(const bool bRuntimeMode = true) const override { return false; }

private:
	FMassEntityQuery SourceEntityQuery;
};
