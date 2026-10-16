#pragma once

#include "CoreMinimal.h"
#include "Nodes/ArcTargetingNode.h"
#include "ArcTargetingSort_AimScreen.generated.h"

USTRUCT(DisplayName = "Sort: Aim + Screen Center")
struct ARCTARGETING_API FArcTargetingSort_AimScreen : public FArcTargetingNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Sort", meta = (ClampMin = "1"))
	float MaxDistance = 5000.f;

	UPROPERTY(EditAnywhere, Category = "Sort", meta = (ClampMin = "1", ClampMax = "180"))
	float MaxAngleDegrees = 60.f;

	// Normalized screen-space radius from center. 0.5 ≈ corner.
	UPROPERTY(EditAnywhere, Category = "Sort", meta = (ClampMin = "0.01", ClampMax = "1"))
	float MaxScreenRadius = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Sort|Weights", meta = (ClampMin = "0"))
	float DistanceWeight = 1.f;

	UPROPERTY(EditAnywhere, Category = "Sort|Weights", meta = (ClampMin = "0"))
	float AngleWeight = 1.f;

	UPROPERTY(EditAnywhere, Category = "Sort|Weights", meta = (ClampMin = "0"))
	float ScreenWeight = 1.f;

	virtual EArcTargetingNodeKind GetKind() const override { return EArcTargetingNodeKind::Sort; }
	virtual void Execute(FArcTargetingContext& Ctx, TArray<FArcTargetCandidate>& Candidates) const override;

#if UE_DEBUG_RENDERING_ENABLED
	virtual void DrawDebug(
		const FArcTargetingContext& Ctx,
		TConstArrayView<FArcTargetCandidate> CandidatesBefore,
		TConstArrayView<FArcTargetCandidate> CandidatesAfter) const override;
#endif
};
