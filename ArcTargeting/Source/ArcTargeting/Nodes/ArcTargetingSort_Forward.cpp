#include "Nodes/ArcTargetingSort_Forward.h"

#include "ArcTargetingContext.h"
#include "ArcTargetingDebug.h"
#include "DebugDraw.h"
#include "GameFramework/Actor.h"

namespace ArcTargetingSortForwardPrivate
{
	FVector ResolveForwardDir(const FArcTargetingContext& Ctx)
	{
		const AActor* SourceActor = Ctx.SourceActor.Get();
		if (SourceActor != nullptr)
		{
			return SourceActor->GetActorForwardVector().GetSafeNormal();
		}
		return Ctx.SourceDirection.GetSafeNormal();
	}
}

void FArcTargetingSort_Forward::Execute(FArcTargetingContext& Ctx, TArray<FArcTargetCandidate>& Candidates) const
{
	using namespace ArcTargetingSortForwardPrivate;

	const FVector ForwardDir = ResolveForwardDir(Ctx);
	const float MaxAngleRad = FMath::DegreesToRadians(FMath::Max(MaxAngleDegrees, 1.f));

	for (FArcTargetCandidate& C : Candidates)
	{
		const FVector ToCand = C.HitResult.ImpactPoint - Ctx.SourceLocation;
		const float Dist = ToCand.Size();

		float AngleTerm = 0.f;
		if (!ForwardDir.IsZero() && Dist > KINDA_SMALL_NUMBER)
		{
			const float CosA = FVector::DotProduct(ForwardDir, ToCand / Dist);
			const float Angle = FMath::Acos(FMath::Clamp(CosA, -1.f, 1.f));
			AngleTerm = 1.f - FMath::Clamp(Angle / MaxAngleRad, 0.f, 1.f);
		}

		C.Score = AngleTerm;
	}

	Candidates.StableSort([](const FArcTargetCandidate& A, const FArcTargetCandidate& B)
	{
		return A.Score > B.Score;
	});
}

#if UE_DEBUG_RENDERING_ENABLED

void FArcTargetingSort_Forward::DrawDebug(
	const FArcTargetingContext& Ctx,
	TConstArrayView<FArcTargetCandidate> CandidatesBefore,
	TConstArrayView<FArcTargetCandidate> CandidatesAfter) const
{
	const FSceneInterface* Scene = GetTargetingDebugScene(Ctx);

	using namespace ArcTargetingSortForwardPrivate;

	const FColor Color = (DebugColor == FColor()) ? FColor::Magenta : DebugColor;
	const FVector ForwardDir = ResolveForwardDir(Ctx);

	UE::DebugDraw::Arrow(Scene, Ctx.SourceLocation, Ctx.SourceLocation + ForwardDir * 200.f, 12.f, Color);

	for (const FArcTargetCandidate& C : CandidatesAfter)
	{
		UE::DebugDraw::Sphere(Scene, C.HitResult.ImpactPoint, 10.f, 12, Color);
	}
}

#endif // UE_DEBUG_RENDERING_ENABLED
