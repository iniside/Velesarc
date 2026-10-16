#pragma once

#include "CoreMinimal.h"
#include "Nodes/ArcTargetingNode.h"
#include "ArcTargetingSort_Priority.generated.h"

USTRUCT(DisplayName = "Sort: Priority")
struct ARCTARGETING_API FArcTargetingSort_Priority : public FArcTargetingNode
{
	GENERATED_BODY()

	virtual EArcTargetingNodeKind GetKind() const override { return EArcTargetingNodeKind::Sort; }
	virtual void Execute(FArcTargetingContext& Ctx, TArray<FArcTargetCandidate>& Candidates) const override;
};
