#pragma once

#include "CoreMinimal.h"
#include "Nodes/ArcTargetingNode.h"
#include "ArcTargetingFilter_IgnoreSourceActor.generated.h"

USTRUCT(DisplayName = "Filter: Ignore Source Actor")
struct ARCTARGETING_API FArcTargetingFilter_IgnoreSourceActor : public FArcTargetingNode
{
	GENERATED_BODY()

	virtual EArcTargetingNodeKind GetKind() const override { return EArcTargetingNodeKind::Filter; }

	virtual void Execute(FArcTargetingContext& Ctx, TArray<FArcTargetCandidate>& Candidates) const override;

#if UE_DEBUG_RENDERING_ENABLED
	virtual void DrawDebug(
		const FArcTargetingContext& Ctx,
		TConstArrayView<FArcTargetCandidate> CandidatesBefore,
		TConstArrayView<FArcTargetCandidate> CandidatesAfter) const override;
#endif
};
