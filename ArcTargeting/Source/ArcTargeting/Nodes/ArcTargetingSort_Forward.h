#pragma once

#include "CoreMinimal.h"
#include "Nodes/ArcTargetingNode.h"
#include "ArcTargetingSort_Forward.generated.h"

USTRUCT(DisplayName = "Sort: Most In Front")
struct ARCTARGETING_API FArcTargetingSort_Forward : public FArcTargetingNode
{
	GENERATED_BODY()

	// Angle (from the character's forward vector) at which score reaches 0. Candidates beyond it floor at 0.
	UPROPERTY(EditAnywhere, Category = "Sort", meta = (ClampMin = "1", ClampMax = "180"))
	float MaxAngleDegrees = 90.f;

	virtual EArcTargetingNodeKind GetKind() const override { return EArcTargetingNodeKind::Sort; }
	virtual void Execute(FArcTargetingContext& Ctx, TArray<FArcTargetCandidate>& Candidates) const override;

#if UE_DEBUG_RENDERING_ENABLED
	virtual void DrawDebug(
		const FArcTargetingContext& Ctx,
		TConstArrayView<FArcTargetCandidate> CandidatesBefore,
		TConstArrayView<FArcTargetCandidate> CandidatesAfter) const override;
#endif
};
