#pragma once

#include "CoreMinimal.h"
#include "Nodes/ArcTargetingNode.h"
#include "Nodes/ArcTargetingTrace_Sphere.h"
#include "Nodes/ArcTargetingTraceCommon.h"
#include "Engine/EngineTypes.h"
#include "ArcTargetingTrace_Cone.generated.h"

USTRUCT(DisplayName = "Trace: Cone")
struct ARCTARGETING_API FArcTargetingTrace_Cone : public FArcTargetingNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Trace")
	EArcTargetingTraceOrigin Origin = EArcTargetingTraceOrigin::ContextLocation;

	UPROPERTY(EditAnywhere, Category = "Trace")
	EArcTargetingTraceDirection DirectionMode = EArcTargetingTraceDirection::ContextDirection;

	UPROPERTY(EditAnywhere, Category = "Trace", meta = (EditCondition = "DirectionMode == EArcTargetingTraceDirection::Explicit", EditConditionHides))
	FVector ExplicitDirection = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, Category = "Trace")
	EArcTargetingTraceFilterMode FilterMode = EArcTargetingTraceFilterMode::Channel;

	UPROPERTY(EditAnywhere, Category = "Trace", meta = (EditCondition = "FilterMode == EArcTargetingTraceFilterMode::Channel", EditConditionHides))
	TEnumAsByte<ECollisionChannel> Channel = ECC_Visibility;

	UPROPERTY(EditAnywhere, Category = "Trace", meta = (EditCondition = "FilterMode == EArcTargetingTraceFilterMode::ObjectType", EditConditionHides))
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes = { EObjectTypeQuery::ObjectTypeQuery3 };

	UPROPERTY(EditAnywhere, Category = "Trace", meta = (EditCondition = "FilterMode == EArcTargetingTraceFilterMode::Profile", EditConditionHides))
	FName ProfileName = NAME_None;

	/** Cone reach measured from the apex along the trace direction. */
	UPROPERTY(EditAnywhere, Category = "Trace", meta = (ClampMin = "0"))
	float Length = 1000.f;

	/** Half-angle of the cone in degrees (angle between axis and the cone surface). */
	UPROPERTY(EditAnywhere, Category = "Trace", meta = (ClampMin = "0.1", ClampMax = "89"))
	float HalfAngleDegrees = 30.f;

	UPROPERTY(EditAnywhere, Category = "Trace")
	int32 CandidatePriority = 0;

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
