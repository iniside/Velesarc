// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DebugDraw.h"
#include "Debug/ArcTownDebugDrawTypes.h"

class AArcTownAttractorVolume;
class AArcTownVolume;

namespace ArcTownDebugDraw
{
#if UE_DEBUG_RENDERING_ENABLED
	ARCTOWN_API void DrawVolume(
		const AArcTownVolume& Volume,
		TConstArrayView<TWeakObjectPtr<const AArcTownAttractorVolume>> CachedAttractors,
		FArcTownDebugDrawFrozenCache& FrozenCache,
		FArcTownDebugDrawGeometryCache& GeometryCache);

	ARCTOWN_API void ResetLogLatches();
#else
	FORCEINLINE void DrawVolume(
		const AArcTownVolume&,
		TConstArrayView<TWeakObjectPtr<const AArcTownAttractorVolume>>,
		FArcTownDebugDrawFrozenCache&,
		FArcTownDebugDrawGeometryCache&) {}

	FORCEINLINE void ResetLogLatches() {}
#endif
}
