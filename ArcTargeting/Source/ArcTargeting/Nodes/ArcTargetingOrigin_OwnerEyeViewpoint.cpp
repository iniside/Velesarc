#include "Nodes/ArcTargetingOrigin_OwnerEyeViewpoint.h"

#include "ArcTargetingContext.h"
#include "ArcTargetingDebug.h"
#include "DebugDraw.h"
#include "GameFramework/Actor.h"

void FArcTargetingOrigin_OwnerEyeViewpoint::Execute(FArcTargetingContext& Ctx, TArray<FArcTargetCandidate>& Candidates) const
{
	const AActor* Source = Ctx.SourceActor.Get();
	if (Source == nullptr)
	{
		return;
	}

	FVector EyeLocation = FVector::ZeroVector;
	FRotator EyeRotation = FRotator::ZeroRotator;
	Source->GetActorEyesViewPoint(EyeLocation, EyeRotation);

	Ctx.SourceLocation = EyeLocation;
	Ctx.SourceDirection = EyeRotation.Vector();
}

#if UE_DEBUG_RENDERING_ENABLED

void FArcTargetingOrigin_OwnerEyeViewpoint::DrawDebug(
	const FArcTargetingContext& Ctx,
	TConstArrayView<FArcTargetCandidate> CandidatesBefore,
	TConstArrayView<FArcTargetCandidate> CandidatesAfter) const
{
	const FSceneInterface* Scene = GetTargetingDebugScene(Ctx);

	const AActor* Source = Ctx.SourceActor.Get();
	if (Source == nullptr)
	{
		return;
	}
	const FColor Color = (DebugColor == FColor()) ? FColor::Cyan : DebugColor;
	UE::DebugDraw::Arrow(Scene, Source->GetActorLocation(), Ctx.SourceLocation, 8.f, Color, 0.5f);
	UE::DebugDraw::Arrow(Scene, Ctx.SourceLocation, Ctx.SourceLocation + Ctx.SourceDirection * 100.f, 12.f, Color, 1.f);
}

#endif // UE_DEBUG_RENDERING_ENABLED
