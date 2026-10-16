#include "ArcTargetingDebug.h"

#if UE_DEBUG_RENDERING_ENABLED

#include "ArcTargetingContext.h"
#include "DebugDraw.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

const FSceneInterface* GetTargetingDebugScene(const FArcTargetingContext& Ctx)
{
	if (const AActor* Source = Ctx.SourceActor.Get())
	{
		return UE::DebugDraw::GetSceneKey(Source->GetWorld());
	}
	if (const UObject* Object = Ctx.SourceObject.Get())
	{
		return UE::DebugDraw::GetSceneKey(Object->GetWorld());
	}
	return nullptr;
}

#endif // UE_DEBUG_RENDERING_ENABLED
