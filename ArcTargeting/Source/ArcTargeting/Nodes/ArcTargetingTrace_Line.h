#pragma once

#include "CoreMinimal.h"
#include "Nodes/ArcTargetingNode.h"
#include "Nodes/ArcTargetingTraceCommon.h"
#include "Engine/EngineTypes.h"
#include "ArcTargetingTrace_Line.generated.h"

USTRUCT(DisplayName = "Trace: Line")
struct ARCTARGETING_API FArcTargetingTrace_Line : public FArcTargetingNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Trace")
	EArcTargetingTraceFilterMode FilterMode = EArcTargetingTraceFilterMode::Channel;

	UPROPERTY(EditAnywhere, Category = "Trace", meta = (EditCondition = "FilterMode == EArcTargetingTraceFilterMode::Channel", EditConditionHides))
	TEnumAsByte<ECollisionChannel> Channel = ECC_Visibility;

	UPROPERTY(EditAnywhere, Category = "Trace", meta = (EditCondition = "FilterMode == EArcTargetingTraceFilterMode::ObjectType", EditConditionHides))
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes = { EObjectTypeQuery::ObjectTypeQuery3 };

	UPROPERTY(EditAnywhere, Category = "Trace", meta = (EditCondition = "FilterMode == EArcTargetingTraceFilterMode::Profile", EditConditionHides))
	FName ProfileName = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Trace", meta = (ClampMin = "0"))
	float MaxDistance = 5000.f;

	UPROPERTY(EditAnywhere, Category = "Trace")
	int32 CandidatePriority = 0;

	UPROPERTY(EditAnywhere, Category = "Trace")
	bool bMultiHit = false;

	UPROPERTY(EditAnywhere, Category = "Trace")
	bool bTraceComplex = false;

	virtual EArcTargetingNodeKind GetKind() const override { return EArcTargetingNodeKind::Trace; }

	virtual void Execute(FArcTargetingContext& Ctx, TArray<FArcTargetCandidate>& Candidates) const override;

#if UE_DEBUG_RENDERING_ENABLED
	virtual void DrawDebug(
		const FArcTargetingContext& Ctx,
		TConstArrayView<FArcTargetCandidate> CandidatesBefore,
		TConstArrayView<FArcTargetCandidate> CandidatesAfter) const override;
#endif
};
