#pragma once

#include "CoreMinimal.h"
#include "Nodes/ArcTargetingNode.h"
#include "Engine/EngineTypes.h"
#include "ArcTargetingFilter_LineOfSight.generated.h"

USTRUCT(DisplayName = "Filter: Line Of Sight")
struct ARCTARGETING_API FArcTargetingFilter_LineOfSight : public FArcTargetingNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Trace")
	TEnumAsByte<ECollisionChannel> Channel = ECC_Visibility;

	UPROPERTY(EditAnywhere, Category = "Trace")
	FVector SourceOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Trace")
	bool bTraceComplex = false;

	virtual EArcTargetingNodeKind GetKind() const override { return EArcTargetingNodeKind::Filter; }

	virtual void Execute(FArcTargetingContext& Ctx, TArray<FArcTargetCandidate>& Candidates) const override;

#if UE_DEBUG_RENDERING_ENABLED
	virtual void DrawDebug(
		const FArcTargetingContext& Ctx,
		TConstArrayView<FArcTargetCandidate> CandidatesBefore,
		TConstArrayView<FArcTargetCandidate> CandidatesAfter) const override;
#endif
};
