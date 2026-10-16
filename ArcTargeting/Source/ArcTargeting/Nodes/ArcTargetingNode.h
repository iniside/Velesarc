#pragma once

#include "CoreMinimal.h"
#include "ArcTargetingPipelineTypes.h"
#include "ArcTargetingNode.generated.h"

struct FArcTargetingContext;

UENUM()
enum class EArcTargetingNodeKind : uint8
{
	Origin,
	Trace,
	Overlap,
	Filter,
	Score,
	Sort,
	Misc,
};

USTRUCT()
struct ARCTARGETING_API FArcTargetingNode
{
	GENERATED_BODY()

	virtual ~FArcTargetingNode() = default;

	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawDebug = false;

	UPROPERTY(EditAnywhere, Category = "Debug", meta = (EditCondition = "bDrawDebug"))
	FColor DebugColor = FColor::Yellow;

	virtual EArcTargetingNodeKind GetKind() const { return EArcTargetingNodeKind::Misc; }

	virtual void Execute(FArcTargetingContext& Ctx, TArray<FArcTargetCandidate>& Candidates) const {}

#if UE_DEBUG_RENDERING_ENABLED
	virtual void DrawDebug(
		const FArcTargetingContext& Ctx,
		TConstArrayView<FArcTargetCandidate> CandidatesBefore,
		TConstArrayView<FArcTargetCandidate> CandidatesAfter) const
	{
	}
#endif
};
