#include "Nodes/ArcTargetingFilter_LineOfSight.h"

#include "ArcTargetingContext.h"
#include "ArcTargetingDebug.h"
#include "ArcTargetingModule.h"
#include "ArcTargetingPipelineTypes.h"
#include "ArcMass/ArcMassEntityLibrary.h"
#include "DebugDraw.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace ArcTargetingFilterLineOfSightPrivate
{
	UWorld* WorldFromContext(const FArcTargetingContext& Ctx)
	{
		if (const AActor* Source = Ctx.SourceActor.Get())
		{
			return Source->GetWorld();
		}
		if (const UObject* Object = Ctx.SourceObject.Get())
		{
			return Object->GetWorld();
		}
		return nullptr;
	}

	bool IsSameTarget(const FArcTargetCandidate& A, const FArcTargetCandidate& B)
	{
		return A.Entity == B.Entity
			&& A.HitResult.GetActor() == B.HitResult.GetActor()
			&& A.HitResult.ImpactPoint == B.HitResult.ImpactPoint;
	}
}

void FArcTargetingFilter_LineOfSight::Execute(FArcTargetingContext& Ctx, TArray<FArcTargetCandidate>& Candidates) const
{
	using namespace ArcTargetingFilterLineOfSightPrivate;

	UWorld* World = WorldFromContext(Ctx);
	if (World == nullptr || Candidates.Num() == 0)
	{
		return;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArcTargetingLineOfSight), bTraceComplex);
	if (const AActor* SourceActor = Ctx.SourceActor.Get())
	{
		Params.AddIgnoredActor(SourceActor);
	}

	const FVector Start = Ctx.SourceLocation + SourceOffset;
	TArray<FArcTargetCandidate> Visible;
	Visible.Reserve(Candidates.Num());

	for (FArcTargetCandidate& Candidate : Candidates)
	{
		FHitResult Hit;
		bool bVisible = !World->LineTraceSingleByChannel(Hit, Start, Candidate.HitResult.ImpactPoint, Channel, Params);
		if (!bVisible)
		{
			const AActor* CandidateActor = Candidate.HitResult.GetActor();
			if (CandidateActor != nullptr && Hit.GetActor() == CandidateActor)
			{
				bVisible = true;
			}
			else if (Candidate.Entity.IsValid())
			{
				EArcMassResult ResolveResult = EArcMassResult::NotValid;
				bVisible = UArcMassEntityLibrary::ResolveHitToEntity(Hit, ResolveResult) == Candidate.Entity;
			}
		}

		if (bVisible)
		{
			Visible.Add(MoveTemp(Candidate));
		}
	}

	UE_LOGF(LogArcTargeting, Verbose, "Filter: Line Of Sight rejected %d of %d candidates.", Candidates.Num() - Visible.Num(), Candidates.Num());
	Candidates = MoveTemp(Visible);
}

#if UE_DEBUG_RENDERING_ENABLED

void FArcTargetingFilter_LineOfSight::DrawDebug(
	const FArcTargetingContext& Ctx,
	TConstArrayView<FArcTargetCandidate> CandidatesBefore,
	TConstArrayView<FArcTargetCandidate> CandidatesAfter) const
{
	using namespace ArcTargetingFilterLineOfSightPrivate;

	const FSceneInterface* Scene = GetTargetingDebugScene(Ctx);
	const FColor Color = (DebugColor == FColor()) ? FColor::Red : DebugColor;
	const FVector Start = Ctx.SourceLocation + SourceOffset;

	for (const FArcTargetCandidate& Before : CandidatesBefore)
	{
		const bool bKept = CandidatesAfter.ContainsByPredicate([&Before](const FArcTargetCandidate& After) -> bool
		{
			return IsSameTarget(Before, After);
		});
		if (bKept)
		{
			continue;
		}
		UE::DebugDraw::Line(Scene, Start, Before.HitResult.ImpactPoint, Color);
		UE::DebugDraw::Box(Scene,
			Before.HitResult.ImpactPoint,
			FVector(15.f, 15.f, 15.f),
			FQuat::Identity,
			Color,
			1.5f);
	}
}

#endif // UE_DEBUG_RENDERING_ENABLED
