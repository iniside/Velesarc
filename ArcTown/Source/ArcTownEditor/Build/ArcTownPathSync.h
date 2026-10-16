// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"

class AArcTownVolume;
struct FArcTownGeneratedPath;

inline constexpr TCHAR ArcTownGeneratedPathPrefix[] = TEXT("ArcTownGenPath_");

namespace ArcTownPathSync
{
	/**
	 * Replaces this town's generated path components with one per generated road. Runs BEFORE
	 * FArcTownInstanceEmitter::Prepare, which reads the component set as its path source.
	 */
	ARCTOWNEDITOR_API void SyncGeneratedPaths(AArcTownVolume& Town, TConstArrayView<FArcTownGeneratedPath> Paths);

	/** Drops this town's generated path components, leaving every authored one standing. */
	ARCTOWNEDITOR_API int32 DestroyGeneratedPaths(AArcTownVolume& Town);
}
