#include "Nodes/ArcTargetingFilter_IgnoreSourceObject.h"

#include "ArcTargetingContext.h"
#include "ArcTargetingPipelineTypes.h"
#include "ArcTargetingDebug.h"
#include "DebugDraw.h"
#include "GameFramework/Actor.h"

void FArcTargetingFilter_IgnoreSourceObject::Execute(FArcTargetingContext& Ctx, TArray<FArcTargetCandidate>& Candidates) const
{
	const AActor* Source = Cast<AActor>(Ctx.SourceObject.Get());
	if (Source == nullptr)
	{
		return;
	}

	// Single-pass by-predicate bulk removal (no per-element RemoveAt).
	Candidates.RemoveAll([Source](const FArcTargetCandidate& C)
	{
		return C.HitResult.GetActor() == Source;
	});
}

#if UE_DEBUG_RENDERING_ENABLED

void FArcTargetingFilter_IgnoreSourceObject::DrawDebug(
	const FArcTargetingContext& Ctx,
	TConstArrayView<FArcTargetCandidate> CandidatesBefore,
	TConstArrayView<FArcTargetCandidate> CandidatesAfter) const
{
	const FSceneInterface* Scene = GetTargetingDebugScene(Ctx);

	const AActor* Source = Cast<AActor>(Ctx.SourceObject.Get());
	const FColor Color = (DebugColor == FColor()) ? FColor::Red : DebugColor;

	for (const FArcTargetCandidate& Before : CandidatesBefore)
	{
		if (Before.HitResult.GetActor() == Source)
		{
			UE::DebugDraw::Box(Scene,
				Before.HitResult.ImpactPoint,
				FVector(15.f, 15.f, 15.f),
				FQuat::Identity,
				Color,
				1.5f);
		}
	}
}

#endif // UE_DEBUG_RENDERING_ENABLED
