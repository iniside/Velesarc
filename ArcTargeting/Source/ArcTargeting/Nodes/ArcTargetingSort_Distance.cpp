#include "Nodes/ArcTargetingSort_Distance.h"

#include "ArcTargetingContext.h"
#include "ArcTargetingDebug.h"
#include "DebugDraw.h"
#include "GameFramework/Actor.h"

namespace ArcTargetingSortDistancePrivate
{
	FVector ResolveReferencePoint(const FArcTargetingContext& Ctx, EArcTargetingSortDistanceReference Ref)
	{
		if (Ref == EArcTargetingSortDistanceReference::SourceActor)
		{
			const AActor* Actor = Ctx.SourceActor.Get();
			if (Actor != nullptr)
			{
				return Actor->GetActorLocation();
			}
		}
		return Ctx.SourceLocation;
	}
}

void FArcTargetingSort_Distance::Execute(FArcTargetingContext& Ctx, TArray<FArcTargetCandidate>& Candidates) const
{
	using namespace ArcTargetingSortDistancePrivate;

	const FVector RefPoint = ResolveReferencePoint(Ctx, Reference);

	for (FArcTargetCandidate& C : Candidates)
	{
		const float Dist = FVector::Dist(RefPoint, C.HitResult.ImpactPoint);
		C.Score = 1.f - FMath::Clamp(Dist / MaxDistance, 0.f, 1.f);
	}

	Candidates.StableSort([](const FArcTargetCandidate& A, const FArcTargetCandidate& B)
	{
		return A.Score > B.Score;
	});
}

#if UE_DEBUG_RENDERING_ENABLED

void FArcTargetingSort_Distance::DrawDebug(
	const FArcTargetingContext& Ctx,
	TConstArrayView<FArcTargetCandidate> CandidatesBefore,
	TConstArrayView<FArcTargetCandidate> CandidatesAfter) const
{
	const FSceneInterface* Scene = GetTargetingDebugScene(Ctx);

	using namespace ArcTargetingSortDistancePrivate;

	const FColor Color = (DebugColor == FColor()) ? FColor::Purple : DebugColor;
	const FVector RefPoint = ResolveReferencePoint(Ctx, Reference);

	UE::DebugDraw::Sphere(Scene, RefPoint, 20.f, 12, Color);

	for (const FArcTargetCandidate& C : CandidatesAfter)
	{
		UE::DebugDraw::Sphere(Scene, C.HitResult.ImpactPoint, 10.f, 12, Color);
		UE::DebugDraw::Line(Scene, RefPoint, C.HitResult.ImpactPoint, Color);
	}
}

#endif // UE_DEBUG_RENDERING_ENABLED
