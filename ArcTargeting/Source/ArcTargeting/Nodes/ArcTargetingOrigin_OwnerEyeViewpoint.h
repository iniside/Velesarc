#pragma once

#include "CoreMinimal.h"
#include "Nodes/ArcTargetingNode.h"
#include "ArcTargetingOrigin_OwnerEyeViewpoint.generated.h"

USTRUCT(DisplayName = "Origin: Owner Eye Viewpoint")
struct ARCTARGETING_API FArcTargetingOrigin_OwnerEyeViewpoint : public FArcTargetingNode
{
	GENERATED_BODY()

	virtual EArcTargetingNodeKind GetKind() const override { return EArcTargetingNodeKind::Origin; }

	virtual void Execute(FArcTargetingContext& Ctx, TArray<FArcTargetCandidate>& Candidates) const override;

#if UE_DEBUG_RENDERING_ENABLED
	virtual void DrawDebug(
		const FArcTargetingContext& Ctx,
		TConstArrayView<FArcTargetCandidate> CandidatesBefore,
		TConstArrayView<FArcTargetCandidate> CandidatesAfter) const override;
#endif
};
