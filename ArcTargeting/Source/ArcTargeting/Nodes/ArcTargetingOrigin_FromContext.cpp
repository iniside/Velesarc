#include "Nodes/ArcTargetingOrigin_FromContext.h"

#include "ArcTargetingContext.h"
#include "ArcTargetingDebug.h"
#include "DebugDraw.h"
#include "GameFramework/Actor.h"

#if UE_DEBUG_RENDERING_ENABLED

void FArcTargetingOrigin_FromContext::DrawDebug(
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
	UE::DebugDraw::Arrow(Scene, Source->GetActorLocation(), Ctx.SourceLocation, 12.f, Color, 1.f);
}

#endif // UE_DEBUG_RENDERING_ENABLED
