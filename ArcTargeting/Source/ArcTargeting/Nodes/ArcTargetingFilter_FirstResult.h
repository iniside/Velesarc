#pragma once

#include "CoreMinimal.h"
#include "Nodes/ArcTargetingNode.h"
#include "ArcTargetingFilter_FirstResult.generated.h"

USTRUCT(DisplayName = "Filter: First Result")
struct ARCTARGETING_API FArcTargetingFilter_FirstResult : public FArcTargetingNode
{
	GENERATED_BODY()

	virtual EArcTargetingNodeKind GetKind() const override { return EArcTargetingNodeKind::Filter; }
	virtual void Execute(FArcTargetingContext& Ctx, TArray<FArcTargetCandidate>& Candidates) const override;
};
