#pragma once

#include "CoreMinimal.h"
#include "Nodes/ArcTargetingNode.h"
#include "ArcTargetingFilter_UniqueTargets.generated.h"

USTRUCT(DisplayName = "Filter: Unique Targets")
struct ARCTARGETING_API FArcTargetingFilter_UniqueTargets : public FArcTargetingNode
{
	GENERATED_BODY()

	virtual EArcTargetingNodeKind GetKind() const override { return EArcTargetingNodeKind::Filter; }
	virtual void Execute(FArcTargetingContext& Ctx, TArray<FArcTargetCandidate>& Candidates) const override;
};
