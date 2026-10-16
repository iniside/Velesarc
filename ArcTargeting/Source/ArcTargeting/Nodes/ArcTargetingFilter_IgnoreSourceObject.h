#pragma once

#include "CoreMinimal.h"
#include "Nodes/ArcTargetingNode.h"
#include "ArcTargetingFilter_IgnoreSourceObject.generated.h"

/**
 * Removes any candidate whose hit actor is the request's SourceObject (cast to AActor).
 *
 * Sibling to FArcTargetingFilter_IgnoreSourceActor, but keyed on SourceObject rather
 * than SourceActor. Used by the building placement loop, where the placement ghost is
 * seeded as SourceObject and must never be its own placement target.
 */
USTRUCT(DisplayName = "Filter: Ignore Source Object")
struct ARCTARGETING_API FArcTargetingFilter_IgnoreSourceObject : public FArcTargetingNode
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
