#pragma once

#include "CoreMinimal.h"
#include "Nodes/ArcTargetingNode.h"
#include "Engine/EngineTypes.h"
#include "ArcTargetingOverlap_Sphere.generated.h"

UENUM()
enum class EArcTargetingOverlapCenter : uint8
{
	FirstCandidate,
	SourceLocation,
};

USTRUCT(DisplayName = "Overlap: Sphere")
struct ARCTARGETING_API FArcTargetingOverlap_Sphere : public FArcTargetingNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Trace")
	EArcTargetingOverlapCenter Center = EArcTargetingOverlapCenter::FirstCandidate;

	UPROPERTY(EditAnywhere, Category = "Trace")
	TEnumAsByte<ECollisionChannel> Channel = ECC_Pawn;

	UPROPERTY(EditAnywhere, Category = "Trace", meta = (ClampMin = "0"))
	float Radius = 250.f;

	UPROPERTY(EditAnywhere, Category = "Trace")
	bool bTraceComplex = false;

	virtual EArcTargetingNodeKind GetKind() const override { return EArcTargetingNodeKind::Overlap; }

	virtual void Execute(FArcTargetingContext& Ctx, TArray<FArcTargetCandidate>& Candidates) const override;

#if UE_DEBUG_RENDERING_ENABLED
	virtual void DrawDebug(
		const FArcTargetingContext& Ctx,
		TConstArrayView<FArcTargetCandidate> CandidatesBefore,
		TConstArrayView<FArcTargetCandidate> CandidatesAfter) const override;
#endif
};
