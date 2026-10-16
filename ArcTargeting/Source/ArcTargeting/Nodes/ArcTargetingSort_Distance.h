#pragma once

#include "CoreMinimal.h"
#include "Nodes/ArcTargetingNode.h"
#include "ArcTargetingSort_Distance.generated.h"

UENUM()
enum class EArcTargetingSortDistanceReference : uint8
{
	SourceLocation,
	SourceActor
};

USTRUCT(DisplayName = "Sort: Distance")
struct ARCTARGETING_API FArcTargetingSort_Distance : public FArcTargetingNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Sort")
	EArcTargetingSortDistanceReference Reference = EArcTargetingSortDistanceReference::SourceLocation;

	// Distance at which score reaches 0. Beyond this, score floors at 0.
	UPROPERTY(EditAnywhere, Category = "Sort", meta = (ClampMin = "1"))
	float MaxDistance = 5000.f;

	virtual EArcTargetingNodeKind GetKind() const override { return EArcTargetingNodeKind::Sort; }
	virtual void Execute(FArcTargetingContext& Ctx, TArray<FArcTargetCandidate>& Candidates) const override;

#if UE_DEBUG_RENDERING_ENABLED
	virtual void DrawDebug(
		const FArcTargetingContext& Ctx,
		TConstArrayView<FArcTargetCandidate> CandidatesBefore,
		TConstArrayView<FArcTargetCandidate> CandidatesAfter) const override;
#endif
};
