// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DebugDraw.h"
#include "Debug/ArcForestDebugDrawTypes.h"

class AArcEcotopeInstancePartitionActor;
class AArcClimateVolume;
class AArcEcotopeVolume;

namespace ArcForestDebugDraw
{
#if UE_DEBUG_RENDERING_ENABLED
	ARCECOTOPE_API void DrawVolume(
		const AArcEcotopeVolume& Volume,
		FArcForestDebugDrawSiteReaderCache& SiteReaderCache,
		FArcForestDebugDrawGeometryCache& GeometryCache,
		TConstArrayView<TWeakObjectPtr<AArcEcotopeInstancePartitionActor>> CachedInstancePartitionActors);

	ARCECOTOPE_API void DrawVolume(
		const AArcClimateVolume& Volume,
		FArcForestDebugDrawGeometryCache& GeometryCache);
#else
	FORCEINLINE void DrawVolume(
		const AArcEcotopeVolume&,
		FArcForestDebugDrawSiteReaderCache&,
		FArcForestDebugDrawGeometryCache&,
		TConstArrayView<TWeakObjectPtr<AArcEcotopeInstancePartitionActor>>) {}

	FORCEINLINE void DrawVolume(
		const AArcClimateVolume&,
		FArcForestDebugDrawGeometryCache&) {}
#endif
}
